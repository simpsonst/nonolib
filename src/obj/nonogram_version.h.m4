#ifndef nonogram_version_HEADER
#define nonogram_version_HEADER
dnl

#define nonogram_VERSION 0
#define nonogram_MINOR 0
#define nonogram_PATCHLEVEL 0

regexp(VERSION, `^\([0-9]+\)\([^.]\)?\.\([0-9]+\)\([^.]\)?\.\([0-9]+\)', ``
#undef nonogram_VERSION
#undef nonogram_MINOR
#undef nonogram_PATCHLEVEL
#define nonogram_VERSION \1
#define nonogram_MINOR \3
#define nonogram_PATCHLEVEL \5'')

#endif
