## Guild Wars 2 BDGM for OSX

Once upon a time (circa ~2016/2017) I created the (infamous)
[BGDM DPS meter](https://web.archive.org/web/20191216040900/http://gw2bgdm.blogspot.com/p/faq.html) for [Guild Wars 2](https://www.guildwars2.com/) which was quite popular,
at one point the severs had 40k concurrent users using the software.

This repo is the OSX version of BDGM which I recently stumbled upon and
decided to never let good code go to waste.

It took countless of hours and many sleepless nights to create this, from
figuring out how to inject/hook OpenGL on OSX (without triggering SIP),
reverse engineering the GW2 OSX client, creating a dynamic signature scanner
to minimize the amount of code updates required every time the game client
was updated and probably many other nuances I can't remember after so long.

AFAIK, GW2 doesn't even have an OSX client anymore, rendering this code
obsolete, nevertheless, I decided to publish this code in the hopes this can
be of use to someone who's looking to create a game overlay add-on for OSX.


![Screenshot1](https://github.com/ibhagwan/gw2bgdm-osx/blob/master/screenshots/BGDM_OSX.jpg)
![Screenshot2](https://github.com/ibhagwan/gw2bgdm-osx/blob/master/screenshots/BGDM_Float.jpg)
