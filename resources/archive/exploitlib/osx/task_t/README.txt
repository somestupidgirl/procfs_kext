This post has been taken from https://googleprojectzero.blogspot.de/2016/10/taskt-considered-harmful.html


task_t considered harmful
Posted by Ian Beer, Project Zero

This post discusses a design issue at the core of the XNU kernel which powers iOS and MacOS. Apple have shipped two iterations of mitigations followed yesterday by a large refactor in MacOS 10.12.1/iOS 10.1. We’ll look at the bugs, how they can be exploited to escape sandboxes and escalate privileges, and how we can defeat each of the mitigations. Every step is accompanied by a working exploit.
Some background on mach ports
Mach ports are multiple-sender, single-receiver message queues maintained by the kernel. Some special mach ports provide the same message-passing API to userspace but messages sent to them are handled synchronously by kernel message handlers. In this sense messages sent to these ports are quite a lot like syscalls.

Task ports are an example of this kind of port. They handle messages which allow senders to manipulate the virtual memory of a task and gain access to its threads. Each task (process) has its own task port. MIG is the name of the tool used to generate the serialization code used by these kernel-owned message ports.
A low-level look at IOKit
When you create a new IOKit user client in userspace you normally call this method from IOKitLib:

kern_return_t
IOServiceOpen(
  io_service_t service,
  task_port_t owningTask,
  uint32_t type,
  io_connect_t *connect );

IOServiceOpen calls the MIG generated serialization code for the io_service_open_extended IPC method and sends that serialized message to the provided IOService port. The mach_msg mach trap notices that this port is owned by the kernel and calls the correct kernel MIG handler for this message rather than queuing it onto the port’s message queue.

The task port passed here is called owningTask; the same name is used throughout the userspace and kernel code. This name was the first thing which made me suspicious. OwningTask implies an ownership relationship which might lead kernel extension developers to believe that behind the scenes IOKit is actually maintaining an ownership relationship which will ensure that the lifetime of this userclient will always be dominated by the lifetime of the owningTask. This is a dangerous assumption, and this blog post is really the fallout from questioning this assumption. Let’s keep following the flow of this code into the kernel. Here’s a snippet from the kernel-side MIG deserialization code for io_service_open_extended:

mig_internal novalue _Xio_service_open_extended(
  mach_msg_header_t *InHeadP,
  mach_msg_header_t *OutHeadP)
{
...
  owningTask = convert_port_to_task(In0P->owningTask.name);

  RetCode = is_io_service_open_extended(
              service,
              owningTask,
              In0P->connect_type,
              In0P->ndr,
              (io_buf_ptr_t)(In0P->properties.address),
              In0P->propertiesCnt, &OutP->result, &connection);

  task_deallocate(owningTask);
...

The kernel has already copied-in all the rights contained in the message so In0P->owningTask.name is actually a pointer to a struct ipc_port and not the mach port’s name as seen from userspace.

Here’s convert_port_to_task:

task_t
convert_port_to_task(
  ipc_port_t port)
{
  task_t task = TASK_NULL;

  if (IP_VALID(port)) {
    ip_lock(port);
    if (ip_active(port) &&
        ip_kotype(port) == IKOT_TASK)
    {
      task = (task_t)port->ip_kobject;
      assert(task != TASK_NULL);
      task_reference_internal(task);
    }
    ip_unlock(port);
  }

  return (task);
}

This checks that the port argument really is a task port object then takes a reference on the task by calling task_reference and returns the task_t pointer. task_t is a typedef for a pointer to a struct task and as you can see from the code it’s a reference-counted object.

is_is_service_open_extended doesn’t do anything with owningTask other than passing it to ::newUserClient:

  res = service->newUserClient(
    owningTask,
    (void *) owningTask,
    connect_type,
    propertiesDict,
    &client );

newUserClient is an IOService method which can be overridden by an IOService if they want to offer multiple userclient types. Otherwise the default implementation will look up the IOService’s IOUserClient subclass class name in the IOKit registry, allocate it via IOKit’s reflection API and call its ::initWithTask method. The default implementation of ::initWithTask also doesn’t do anything with owningTask.

Having looked through the code this far it looks like it’s not the case that by default the owningTask is going to hold a reference on the userclient (which would prevent the userclient taking a reference on the task and causing a reference cycle.) In fact it’s clearly quite the opposite; the userclient must take a reference on the owningTask if it wishes to keep a reference to the owningTask - there’s no implicit ownership relationship at all.
Checking the docs
There aren’t a whole lot of resources for writing OS X kernel extensions. Apple does publish a sample kext on their developer site called AppleSamplePCI which provides examples of various IOKit design patterns. Here’s the AppleSamplePCI.kext implementation of initWithTask:

bool SamplePCIUserClientClassName::initWithTask(
  task_t owningTask,
  void* securityID,
  UInt32 type,
  OSDictionary* properties)
{
  bool success = super::initWithTask(owningTask,
                                     securityID,
                                     type,
                                     properties);
  fTask = owningTask;
  fDriver = NULL;

  return success;
}

The sample userclient stores the owningTask argument in the fTask member variable without taking a reference. Without that reference there’s no guarantee that the task struct pointed to by fTask hasn’t been freed after this method returns. Looking through the rest of the sample kext we can see that some external methods use the fTask pointer to create memory descriptors - if we can get the task struct pointed to by fTask to be freed these will be using a dangling pointer.

Opening up a handful of other OS X kexts in IDA it’s pretty clear that lots of them follow this anti-pattern of holding a task_t pointer without taking a reference.
Creating a dangling task_t
Mach messages provide very flexible and powerful IPC building blocks. One of the neat things you can do is send other processes send-rights to mach ports for which you hold send or receive rights.

Because task ports give you complete control over other tasks the api to request the task port for another task (task_for_pid) is privileged but since all tasks have send rights to their own task ports if we have code execution in two tasks we can send a send-right to the second task’s task port to the first.

In this case we’ll use the technique outlined by Robert Sesek to create a shared mach port between a parent and forked child by stashing a send-right in the bootstrap_port special port slot. After the fork the child can recover this stashed port, restore the bootstrap port and set up a bi-directional IPC channel over which it can send its task port back to the parent.

Triggering the UaF in the vulnerable anti-pattern looks like this:

parent forks off a child
child sends its task port back to its parent
child spins
parent receives child’s task port and creates a vulnerable IOKit userclient passing the child’s task port as owningTask
parent destroys its send right to the child’s task port
parent kills child, freeing the task struct of the child
parent has a userclient with a dangling task struct pointer
A first exploit
Looking through the IOKit drivers which had this bug, one jumped out as being particularly interesting - IOSurfaceRootUserClient. Here’s what the Apple developer docs have to say about IOSurface:

The IOSurface framework provides a framebuffer object suitable for sharing across process boundaries. It is commonly used to allow applications to move complex image decompression and draw logic into a separate process to enhance security.

In reality IOSurfaces are just wrappers around shared memory buffers. On OS X we can talk to the IOSurface kernel extension from inside the Safari renderer sandbox and the Chrome GPU sandbox, amongst others.

The IOSurfaceRootUserClient class has exactly the same anti-pattern as we saw in the AppleSamplePCI client where the userclient stores a copy of the owningTask pointer as a member variable without taking a reference. Some reversing tells us that external method 0 of IOSurfaceRootUserClient is create_surface which takes a dictionary of key-value parameters used to create a shared memory object that other processes can map into their address spaces. By passing the following keys and values we can get IOSurfaceRootUserClient to wrap existing userspace pages in an IOSurface rather than allocating a new buffer:

  IOSurfaceAddress:   base_address
  IOSurfaceAllocSize: size
  IOSurfaceIsGlobal:  true

The IOSurface object actually just wraps an IOMemoryDescriptor which is allocated in IOSurface::allocate by calling:

IOMemoryDescriptor *
IOMemoryDescriptor::withAddressRange(
  mach_vm_address_t address,
  mach_vm_size_t length,
  IOOptionBits   options,
  task_t         task);

The final task_t task argument to IOMemoryDescriptor::withAddressRange defines which task’s virtual memory the descriptor should be created for. IOSurface passes the member variable storing its copy of owningTask here, on which it doesn’t hold a reference! If we can get that task struct memory to be freed (by the original task exiting), reallocated (by another task starting) and used as the task struct for a more privileged task then this IOMemoryDescriptor will believe it’s wrapping a portion of the current process’s address space when it’s actually wrapping a portion of that other more privileged task’s virtual memory in the IOMemoryDescriptor which backs this IOSurface.

Setting IOSurfaceIsGlobal=true makes that surface available to other processes so that by calling external method 6 (lookup_surface) on another IOSurfaceRootUserClient created with our own legitimate task port as the owningTask we can build a primitive which allows us to map arbitrary portions of other process’s address spaces into our own :-)

Since the IOMemoryDescriptor is actually creating shared memory mappings of those pages we can write to them and those writes will also be reflected in the other process. IOSurfaceRootUserClient doesn’t allow us to map executables pages from the victim but we can still map for example the __DATA segment of libraries. This is made easier by the shared library cache being at the same virtual address in all processes.
Putting the exploit together
We need a way to get the task struct reused by a more privileged process and then we need something to overwrite in the target to get code execution.

Task structs are allocated from their own kernel heap zone which greatly simplifies things. We can just kill the child and fork and exec a few suid-root binaries and they are very likely to re-use the same memory pointed to by the dangling task_t.

For the overwrite target I chose to target the __cleanup pointer in libc. This will be called when the process exits.  We can play a few tricks to block the binary just before it exits by setting its stderr file descriptor to a full pipe and forcing it to write an error message giving us plenty of time to exploit the bug in the parent process and overwrite the __cleanup pointer before emptying the pipe in the parent. I chose to point the function pointer to a gadget which adds a large constant to RSP and returns. Doing this moves the stack pointer up into argv and since we exec’ed this binary I put a simple ROP stack there to call setuid(0) and execve /bin/bash. The ROP payload is prefixed with a large number of ret-slide gadgets so it should be stable across most versions of OS X.

You can download this exploit and check out the original bug report.

Since this bug also allows us to gain any entitlements we want as well as root it’s easy to use it to defeat kernel code signing on OS X and load an unsigned kernel extension. See the exploit for CVE-2016-1757 for one way to do this.

Although this exploit uses fork and execve they aren’t actually required - the only prerequisite for triggering the bug is that you need code execution in two co-operating processes which can send mach messages to each other, and for this particular bug to be able to talk to IOSurface. Damien DeVille has a blog post discussing ways of achieving this from within the app sandbox on iOS using application groups. It’s also not necessary to exec a suid-root binary: we could cause the freed task struct to be reused by another more privileged task by looking up a mach service via launchd or deliberately crashing and causing launchd to run the CrashReporter.

Many individual instances of this bug were fixed in OS X 10.11.6/iOS 9.3.3 and Apple shipped a mitigation to prevent passing other task’s task ports to certain IOKit methods.
Stepping back
This use-after-free bug is quite fun but it obscures a far deeper and more concerning issue. If the IOSurfaceRootUserClient now calls task_reference() on owningTask, and owningTask has to be the original creator of the userclient, is there still a bug?

Earlier this year osxreverser@ and I both independently published research about a problem with the execve syscall. In that case there was a race condition due to the order in which execve performed certain operations when loading a suid binary which left a small race window between the new memory map being created and the old task port being invalidated.

There’s a far more fundamental problem: the execve syscall doesn’t actually create a new task struct, even when it executes a more privileged suid binary. It just modifies the existing task struct in-place and any objects which previously had a task_t pointer now have one to a more privileged task.

This isn’t temporal memory safety - there’s no use-after-free involved. Lets look in detail at why that’s such a large problem for XNU.
XNU’s Neither Unix Nor Mach
In a pure Mach microkernel invalidating the old task port would be sufficient to prevent any other process from maintaining control of a task across a privilege-escalating exec, but XNU isn’t a microkernel. Earlier we looked at the kernel function convert_port_to_task which takes a mach task port and converts it into a task struct pointer. This pointer can then be used and passed around within the kernel without all the overhead of sending messages. For example when IOKit wants to manipulate the virtual memory of a process, rather than having to send a mach message to the mach_vm MIG subsystem (which it could theoretically do) it instead directly calls the responsible kernel function.

Another way to think of this is that all the MIG subsystems which live in the kernel (IOKit, mach_vm, tasks, threads, semaphores etc) are directly linked against each other. They can simply call the target functions rather than going via the MIG IPC layer. This is obviously massively faster, but comes at a cost.
Every task_t pointer is a potential security bug
The tradeoff is that now there’s no central point where access to a resource can be cut off. In the kernel they can’t just invalidate the task port when a privileged exec happens and expect that to work because the kernel-internal MIG subsystems don’t use task ports, they just translate between task ports and task struct pointers once at the user/kernel boundary. The kernel has no idea where all the kernel pointers to a task’s task struct are; it can’t hope to invalidate them.

This is a much bigger problem than the original reference counting bug. When a privilege-escalating exec takes place execve doesn’t create a new process; the task struct stays the same, just the privileges change. This means that every single task_t pointer in the kernel is a potential security bug - there’s no locking mechanism to let you assert that the privileges of a task struct haven’t changed since you got access to it and just because kernel code got access to a task struct at one time doesn’t mean it should have access later.
On the heap: rewriting the IOSurface exploit
We actually only need to slightly tweak the original IOSurface exploit to work even with the correct task_reference(owningTask) call. Instead of the child passing its task port back to the parent we’ll instead create the IOSurfaceRootUserClient in the child (correctly using the child’s own task port) and pass that userclient port back to the parent.

The child can then execve a suid-root binary which will set the EUID of the task to 0 without freeing the task struct. The parent still has a send right to the IOSurfaceRootUserClient, and that userclient’s owningTask now has EUID 0. The parent can then proceed as before, blocking the child, mapping the target’s libc __DATA segment, overwriting a function pointer and unblocking the child so that it tries to exit and executes the ROP stack. This new exploit also defeats the mitigation added in 10.11.6 which stops the creation of userclients with other task’s task ports.

Note that there are no failure cases for this exploit - there’s no race to win and no use-after-free which could go wrong. The exploit should work on all OS X versions <= 10.11.6.

This primitive is slightly less powerful than the use after free, which could break you out of very restrictive sandboxes, as you do need to call execve. These IOKit objects which store task_t pointers on the heap are really just the top of the iceberg though.
On the stack: exploiting task_threads
Back closer to the user/kernel boundary as soon as convert_port_to_task has converted a task port received from userspace into a task struct pointer, that task could exec a suid-root or entitled binary and increase its privileges. Even if that task struct pointer isn’t stored on the heap there could still be an exploitable bug. Once case of this is the kernel MIG task_threads method:

kern_return_t
task_threads(
  task_t target_task,
  thread_act_array_t *act_list,
  mach_msg_type_number_t *act_listCnt );

Given a send right to a task port this method returns send rights to the thread ports for each of the threads in that task. Here’s a snippet from MIG auto-generated code in the kernel:

  target_task = convert_port_to_task(
    In0P->Head.msgh_request_port); // (1)
  RetCode = task_threads(
              target_task,
              (thread_act_array_t *)&(OutP->act_list.address),
              &OutP->act_listCnt);
  task_deallocate(target_task);

Here we see that the task port is converted into the underlying task struct pointer which is then stored in the target_task local variable which lives for the duration of this function call.

Here’s the relevant code from task_threads:

  task_threads(
    task_t task,
    thread_act_array_t *threads_out,
    mach_msg_type_number_t *count)
  {
    ...
    for (thread = (thread_t)queue_first(&task->threads);
         i < actual;
         ++i, thread = (thread_t)queue_next(&thread->task_threads)) {
      thread_reference_internal(thread);
      thread_list[j++] = thread;
    }

    ...

      for (i = 0; i < actual; ++i)
        ((ipc_port_t *) thread_list)[i] = convert_thread_to_port(thread_list[i]); // (2)
      }
    ...
  }


This code iterates through the list of threads collecting the struct thread pointers then converts those struct threads to thread ports and returns. There are a handful of locks in the code but they’re not relevant.

What happens if that task is exec-ing a suid root binary at the same time?

The relevant parts of the exec code are these two points in ipc_task_reset and ipc_thread_reset:

  void
  ipc_task_reset(
    task_t    task)
  {
    ipc_port_t old_kport, new_kport;
    ipc_port_t old_sself;
    ipc_port_t old_exc_actions[EXC_TYPES_COUNT];
    int i;

    new_kport = ipc_port_alloc_kernel();
    if (new_kport == IP_NULL)
      panic("ipc_task_reset");

    itk_lock(task);

    old_kport = task->itk_self;

    if (old_kport == IP_NULL) {
      itk_unlock(task);
      ipc_port_dealloc_kernel(new_kport);
      return;
    }

    task->itk_self = new_kport;
    old_sself = task->itk_sself;
    task->itk_sself = ipc_port_make_send(new_kport);
    ipc_kobject_set(old_kport, IKO_NULL, IKOT_NONE); // (3)

This is followed by a call to ipc_thread_reset:

  ipc_thread_reset(
    thread_t  thread)
  {
    ipc_port_t old_kport, new_kport;
    ipc_port_t old_sself;
    ipc_port_t old_exc_actions[EXC_TYPES_COUNT];
    boolean_t  has_old_exc_actions = FALSE; 
    int      i;

    new_kport = ipc_port_alloc_kernel();
    if (new_kport == IP_NULL)
      panic("ipc_task_reset");

    thread_mtx_lock(thread);

    old_kport = thread->ith_self;

    if (old_kport == IP_NULL) {
      thread_mtx_unlock(thread);
      ipc_port_dealloc_kernel(new_kport);
      return;
    }

    thread->ith_self = new_kport; // (4)

Let's call the process which is doing the exec process B and the process calling task_threads() process A and imagine the following interleaving of execution:

A:
  target_task = convert_port_to_task(
    In0P->Head.msgh_request_port); // (1)
  A gets pointer to process B's task struct on the stack

B:
  ipc_kobject_set(old_kport, IKO_NULL, IKOT_NONE); // (3)
  B is execing a suid binary and invalidates the old task port so that it no longer has a task struct pointer

B:
  thread->ith_self = new_kport; // (4)
  B allocates new thread ports and sets them up

A:
  ((ipc_port_t *) thread_list)[i] = convert_thread_to_port(thread_list[i]); // (2)
  A reads and converts the new thread port objects for B’s privileged threads giving A a privileged thread port

Send rights to a thread port give you complete register control. The exploit proceeds in a similar fashion to the previous two except that once it’s got the thread port it can directly point RIP to the gadget address rather than overwriting a function pointer. This race window is quite tight as is requires a very particular interleaving of execution but it does work. Check out the exploit and the original bug report.
Mitigations round 2
The release of iOS 10/MacOS 10.12 brought another round of mitigations to defeat.
Firstly on the IOKit side userclient lifetime is now directly tied to that of the creating task. Secondly there’s a mitigation in ipc_kobject server to detect when a MIG kernel method has raced an execve syscall and force the method to fail if a race was detected:

/*
 * Check if the port is a task port, if its a task port then
 * snapshot the task exec token before the mig routine call.
 */
ipc_port_t port = request->ikm_header->msgh_remote_port;
if (IP_VALID(port) && ip_kotype(port) == IKOT_TASK) {
  task = convert_port_to_task_with_exec_token(port, &exec_token);
}

(*ptr->routine)(request->ikm_header, reply->ikm_header);

/* Check if the exec token changed during the mig routine */
if (task != TASK_NULL) {
  if (exec_token != task->exec_token) {
    exec_token_changed = TRUE;
  }
  task_deallocate(task);
}

There are three flaws with this mitigation:
It only inspects the first argument, there are kernel MIG methods which take a task port in a different position.
It only checks for task ports, these issues also affect thread_ports in a similar way
It only mitigates cases of the bug where we need to get the resources which are returned by the MIG call (eg ports.) There are plenty of other methods which actually directly modify the process state rather than returning new ports.
Exploiting the 2nd round mitigations
Although we can no longer directly get a new thread port via task_threads there are still some more roundabout ways to get it. We just need an API which modifies state rather than directly returning something useful (like a task port) to us.

task_set_exception_port allows us to set a the exception port for a task. When an exception is raised (for example by accessing invalid memory) the kernel will send an exception message to the registered exception handler. Importantly for us that exception message contains the task and thread ports for the thread which caused the exception.

Like almost all places in the kernel with a task_t on the stack this api has a vulnerable race condition. In process A we’ll keep calling task_set_exception_ports() passing B’s task port while B execve’s a suid binary:

mig_internal novalue _Xtask_set_exception_ports(
  mach_msg_header_t *InHeadP,
  mach_msg_header_t *OutHeadP) {
...
  task = convert_port_to_task(In0P->Head.msgh_request_port); // (1)

  OutP->RetCode =
    task_set_exception_ports(task,
                             In0P->exception_mask,
                             In0P->new_port.name,
                             In0P->behavior,
                             In0P->new_flavor);
  task_deallocate(task);
...



kern_return_t
task_set_exception_ports(
  task_t                task,
  exception_mask_t      exception_mask,
  ipc_port_t            new_port,
  exception_behavior_t  new_behavior,
  thread_state_flavor_t new_flavor)
{
...
  itk_lock(task); // (2)

  for (i = FIRST_EXCEPTION; i < EXC_TYPES_COUNT; ++i) {
    if ((exception_mask & (1 << i)) ) {
      old_port[i] = task->exc_actions[i].port;
      task->exc_actions[i].port = ipc_port_copy_send(new_port); // (3)
      task->exc_actions[i].behavior = new_behavior;
      task->exc_actions[i].flavor = new_flavor;
      task->exc_actions[i].privileged = privileged;
    }
...
  itk_unlock(task);
...

Process B calls execve to exec a privileged suid binary:

ipc_task_reset(
  task_t  task)
{
...
  itk_lock(task); // (4)
...
  ip_lock(old_kport);
  ipc_kobject_set_atomically(old_kport, IKO_NULL, IKOT_NONE); // (5)
  task->exec_token += 1;
  ip_unlock(old_kport);

  ipc_kobject_set(new_kport, (ipc_kobject_t) task, IKOT_TASK);

  for (i = FIRST_EXCEPTION; i < EXC_TYPES_COUNT; i++) {
...
    if (!task->exc_actions[i].privileged) {
      old_exc_actions[i] = task->exc_actions[i].port;
      task->exc_actions[i].port = IP_NULL; // (6)
    }
  }

  itk_unlock(task); //(7)

We’re looking for the following interleaving:

A:
task = convert_port_to_task(In0P->Head.msgh_request_port); // (1)

B:
itk_lock(task); // (4)

ipc_kobject_set_atomically(old_kport, IKO_NULL, IKOT_NONE); // (5)

task->exc_actions[i].port = IP_NULL; // (6)

itk_unlock(task); //(7)

A:
itk_lock(task); // (2)

task->exc_actions[i].port = ipc_port_copy_send(new_port); // (3)

This race condition is far easier to win than the task_threads case as the locks make sure that everything lines up nicely for us. We just need to call task_set_exception_ports in a loop and hope that (1) gets called by A just before B takes the task lock at (4). In practise the exploit wins the race in a few milliseconds.

The final trick is to actually make sure that if we win the race we force the child to cause an exception and send us its task and thread ports. We can do this by calling setrlimit(RLIMIT_STACK) with a very small value just before execing the suid target. This means that the binary will be run with a tiny stack and will almost immediately segfault.

In the parent once the task_set_exception_port call has failed we try to receive on the exception port with a short timeout. If a message is received then we won the race and that message contains the task and thread ports for an euid 0 process. In this case the exploit allocates some RWX memory in the task and copies a shellcode stub into there which does this:

  struct rlimit lim = {0x1000000, 0x1000000};
  setrlimit(RLIMIT_STACK, lim);
  setuid(0);
  char* argv[2] = {"/bin/bash", 0};
  execve("/bin/bash", argv, 0);

This shellcode sets the stack size back to a large value, does a setuid(0) to prevent bash dropping privileges and executes a shell.

This exploit should work reliably on all versions of MacOS/OS X 10.12.0 and below.
The final fix
This isn’t an easy bug class to fix. Due to the design of XNU there are task_t pointers everywhere and the underlying issue affects more than just task_t; threads suffer from the same issue. Apple decided to refactor the execve code to allocate new task and thread structures when loading a binary which should fix the underlying issue. This is a considerable amount of work, kudos to Apple for the engineering effort they put into fixing these bugs and I look forward to the release of the MacOS 10.12.1 XNU source to see the new code.


