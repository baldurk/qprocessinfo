# QProcessInfo
Quick Qt class to enumerate running processes on a system

# Building
Should just be able to drop into any Qt project and go, it deliberately doesn't have any dependencies that shouldn't already be there (entirely Qt code on unix, only kernel32.dll on windows).

# TODO
Only tested briefly on Windows and Linux, enough for my purposes. On non-windows the window titles are always empty, at least for X11 they could probably be retrieved somehow. Ditto on mac.

# License
2-clause BSD license. Go hog wild!
