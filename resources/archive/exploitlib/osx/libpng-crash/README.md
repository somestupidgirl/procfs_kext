this bug was discovered by:

https://www.landaire.net/blog/apple-imageio-denial-of-service/

the issue is libpng-related, so linux is also affected.
-----------------------------------------------------------------------------------------------------------------------------------------------------------

Apple ImageIO Denial of Service


#png #bleed #vulnmarketing #infosec #hype. click the image if you're using Safari

Application Services is a framework in iOS and OS X which provides what’s known as the Image I/O framework. ImageIO itself is a collection of utilities and data types for parsing various image formats. It’s used in many OS X and iOS applications including:

Tweetbot
Safari
Messages
Mail
Preview
Some popular applications that do not use ImageIO include:

Chrome
Firefox
Telegram
Given the impact of media processing bugs such as Stagefright I decided that this would be a good target for fuzzing. I created a simple application that uses some various features of ImageIO, grabbed a small PNG I had on my desktop which happened to be a screenshot, and let afl run.

I checked on my fuzzer after 30 minutes and already a crash!

Here’s the code used: Gist (this is almost verbatim Apple’s ImageIO sample).

Note: a tl;dr is available at the bottom

PNG Structure

Before I dive into the vulnerability I’ll talk a little bit about the structure of PNG files. PNGs are comprised of the header and chunks with chunks starting at offset 0x8 in the file. Each chunk contains the chunk type (a 4-character ASCII string), some data, and a CRC32 of that data.

The chunk structure can be represented as the following Go struct:

type Chunk struct {
	Length uint32
	Type   [4]byte
	Data   []byte
	CRC    uint32
}
The PNG specification defines 4 “critical” chunk types and 14 “ancillary” chunk types for a total of 18 formally defined types which you can find here. Any chunks not specified here are considered unknown chunks that can be handled by the decoder.

The vulnerability

Inspecting the crash log of the application makes it pretty obvious that we have a null pointer dereference:

Exception Type:  EXC_BAD_ACCESS (SIGSEGV)
Exception Subtype: KERN_INVALID_ADDRESS at 0x0000000000000000
Triggered by Thread:  0
The stack trace tells us a little more though:

Thread 0 name:  Dispatch queue: com.apple.main-thread
Thread 0 Crashed:
0       ImageIO                       	0x18699a618 0x186994000 + 0x6618	// read_user_chunk_callback + 0x13c
1       ImageIO                       	0x18699a610 0x186994000 + 0x6610	// read_user_chunk_callback + 0x134
2       ImageIO                       	0x18699a328 0x186994000 + 0x6328	// png_handle_unknown + 0x44
3       ImageIO                       	0x18699961c 0x186994000 + 0x561c	// _cg_png_read_info + 0x11c
4       ImageIO                       	0x186997b38 0x186994000 + 0x3b38	// initImagePng + 0x654
5       ImageIO                       	0x186996c98 0x186994000 + 0x2c98	// makeImagePlus + 0x4e4
6       ImageIO                       	0x186996240 0x186994000 + 0x2240	// CGImageSourceCreateImageAtIndex + 0xb8
7       UIKit                         	0x18ad473d8 0x18abe0000 + 0x1673d8	// _UIImageRefFromData + 0x1a8
8       UIKit                         	0x18aed2ff8 0x18abe0000 + 0x2f2ff8	// -[UIImage(UIImagePrivate) _initWithData:preserveScale:cache:] + 0x78
9       UIKit                         	0x18ad471fc 0x18abe0000 + 0x1671fc	// +[UIImage imageWithData:] + 0x48
read_user_chunk_callback is where the crash occurs. A quick Google search yields some results in libpng. Some more digging around the source code (which at the time of this writing I don’t have available, so no links, sorry) lead me to conclude that Apple under the hood uses libpng for the PNG files and the crashing image has an unknown (non-standard) chunk that’s related to the crash.

A quick diff between the input file and crasher revealed that the input file had a chunk with data size of 0. What’s happening here is:

libpng hits an unknown chunk
The custom chunk callback is called
Apple’s own internal method which returns a pointer to the chunk returns null since there’s no data
There’s no check on the chunk pointer returned
Oops
Impact

This bug can be triggered any time a PNG file is being processed. So really, anything that processes the image can be caused to crash. Some examples:

Receiving the malicious image via text message with message previews turned on will crash SpringBoard on iOS
Entering a message thread containing the image will crash the messages app
Opening an email containing the image will crash the mail client
Posting a link to the image will crash some third-party Twitter clients which try to load the image
Visiting a page containing the image will crash Safari’s content renderer
Affected versions

The only devices I had available at the time were an iPad on iOS 7.1, my iPhone on iOS 9.0.2, and my Mac on OS X 10.11.2. All of these devices were vulnerable. It’s reasonable to assume that this bug goes back quite far.

iOS	OS X
Minimum tested version	7.1	9.3.?
Fixed version	9.3.1	10.11.?
Other findings

In exploring this bug I thought it was useful to test out various applications to see how they would handle this type of invalid image. Since the chunks all contain a CRC32 of the data, you cannot modify the image outright and then upload it to almost any service. Twitter and imgur were two hosts I tried and they both would not accept the image because of the invalid CRC32. I wrote a simple Go utility that iterates the chunks and fixes any invalid CRCs.

After fixing the chunk both hosts accepted the image just fine. imgur (and likely most other hosts) do not bother to strip unknown chunks so uploading the image to imgur puts those users at risk. Twitter and Facebook on the other hand will re-encode any image as a JPEG which will obviously remove the malicious chunk.

I think that this is interesting and really important for privacy-concerned people. Before investigating this bug I had not considered additional chunks not being stripped when uploading an image to services. It makes sense, but it seems like this would be an easy way for vendors to hide additional info about the device which took the image outside of the EXIF-related chunks and have them survive re-encoding.

Earlier I mentioned that the image I grabbed was some random screenshot I had sitting on my desktop. It turns out that screenshots taken on OS X include a custom iDOT chunk which contain some additional 28 bytes of data. The structure for the screenshots I took were of form:

num_entries uint32 (usually 0x00000002)
unk1        uint32 (usually 0x00000000)
width       uint32
unk2        uint32 (usually 0x00000028)
width       uint32
width       uint32
unk3        uint32
Yes, the width is repeated 3 times.

I couldn’t figure out what the unknown data is and wasn’t too motivated to reverse engineer the rest of the function so if anyone reading this has any ideas, please ping me on Twitter (@landaire).

Timeline

Dec 16, 2015: Reported vulnerability to vendor
Dec 17, 2015: Vendor acknowledged vulnerability
Dec 27, 2015: Posted pic to Twitter to see what would happen
Dec 27, 2015: Vendor said the bug was undergoing triage
Mar 21, 2016 (91 days since diclosure): iOS 9.3 released and bug still not fixed, status update requested
Mar 22, 2016: Vendor notified me that a fix is “in progress”
April 22, 2016: Public disclosure
If you’d like to have a sample image, you can find it here. NOTE if you are using Safari, your browser will crash. If you aren’t, you will see a screenshot of a Kanye tweet.

tl;dr a custom PNG chunk with a 0-length data field will trigger null pointer dereference causing the application to crash similar to the CoreText crash from 2013.

shoutouts to my boys ed snowden and james comey
