#! /bin/sh
rm -f contrib/msub/msub
rm -f src/client/xpilot
rm -f src/mapedit/xp-mapedit
rm -f src/replay/xp-replay
rm -f src/server/xpilots
find . -name \*~ | xargs rm -f
find . -name \*.bak | xargs rm -f
find . -name \*._man | xargs rm -f
find . -name \*.o | xargs rm -f
find . -name \*.a | xargs rm -f
rm -f contrib/Makefile
rm -f contrib/msub/Makefile
rm -f doc/Makefile
rm -f doc/man/Makefile
rm -f lib/Makefile
rm -f lib/maps/Makefile
rm -f lib/maps/Makefile.bak
rm -f lib/textures/Makefile
rm -f lib/textures/Makefile.bak
rm -f Makefile
rm -f src/client/Makefile
rm -f src/common/Makefile
rm -f src/common/NT/bindist/Makefile
rm -f src/common/NT/bindist/READMEbin.txt
rm -f src/common/NT/bindist/ServerMOTD.txt
rm -f src/common/NT/Makefile
rm -f src/Makefile
rm -f src/mapedit/Makefile
rm -f src/replay/Makefile
rm -f src/server/Makefile
