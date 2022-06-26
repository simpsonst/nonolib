all::

FIND=find
SED=sed
XARGS=xargs
M4=m4
CAT=cat
CP=cp
GETVERSION=git describe --tags

PREFIX=/usr/local

VERSION:=$(shell $(GETVERSION) 2> /dev/null)

## Provide a version of $(abspath) that can cope with spaces in the
## current directory.
myblank:=
myspace:=$(myblank) $(myblank)
MYCURDIR:=$(subst $(myspace),\$(myspace),$(CURDIR)/)
MYABSPATH=$(foreach f,$1,$(if $(patsubst /%,,$f),$(MYCURDIR)$f,$f))

sinclude $(call MYABSPATH,config.mk)
sinclude nonolib-env.mk

###

libraries += nonogram
nonogram_mod += grid
nonogram_mod += line
nonogram_mod += puzzle
nonogram_mod += rule
nonogram_mod += solver
nonogram_mod += conf
nonogram_mod += fast
nonogram_mod +=	complete
nonogram_mod += null
nonogram_mod += oddones
nonogram_mod += olsak
nonogram_mod += fcomp
nonogram_mod += cache

headers += nonogram.h
headers += nonocache.h
headers += nonogram_version.h

test_binaries += testline
testline_obj += testline
testline_obj += $(nonogram_mod)

test_binaries += testio
testio_obj += testio.o
testio_obj += $(nonogram_mod)

SOURCES:=$(patsubst src/obj/%,%,$(filter-out $(headers),$(shell $(FIND) src/obj \( -name "*.c" -o -name "*.h" \))))

riscos_zips += nonolib
nonolib_appname=!NonoLib
nonolib_rof += !Boot,feb
nonolib_rof += !Help,fff
nonolib_rof += $(call riscos_hdr,$(headers))
nonolib_rof += $(call riscos_src,$(SOURCES))
nonolib_rof += $(call riscos_lib,$(libraries))
# nonolib_rof += CONTACT,fff
# nonolib_rof += HISTORY,fff
nonolib_rof += COPYING,fff
nonolib_rof += VERSION,fff
# nonolib_rof += TODO,fff

include binodeps.mk

all:: installed-libraries

lc=$(subst A,a,$(subst B,b,$(subst C,c,$(subst D,d,$(subst E,e,$(subst F,f,$(subst G,g,$(subst H,h,$(subst I,i,$(subst J,j,$(subst K,k,$(subst L,l,$(subst M,m,$(subst N,n,$(subst O,o,$(subst P,p,$(subst Q,q,$(subst R,r,$(subst S,s,$(subst T,t,$(subst U,u,$(subst V,v,$(subst W,w,$(subst X,x,$(subst Y,y,$(subst Z,z,$1))))))))))))))))))))))))))

ifneq ($(filter true t y yes on 1,$(call lc,$(ENABLE_RISCOS))),)
install:: install-riscos
all:: riscos-zips
endif


$(BINODEPS_OUTDIR)/riscos/!NonoLib/!Help,fff: README.md
	$(MKDIR) "$(@D)"
	$(CP) "$<" "$@"

$(BINODEPS_OUTDIR)/riscos/!NonoLib/COPYING,fff: LICENSE.txt
	$(MKDIR) "$(@D)"
	$(CP) "$<" "$@"

$(BINODEPS_OUTDIR)/riscos/!NonoLib/VERSION,fff: VERSION
	$(MKDIR) "$(@D)"
	$(CP) "$<" "$@"

ifneq ($(VERSION),)
prepare-version::
	@$(MKDIR) tmp/
	@$(ECHO) $(VERSION) > tmp/VERSION

tmp/VERSION: | prepare-version
VERSION: tmp/VERSION
	@$(CMP) -s '$<' '$@' || $(CP) '$<' '$@'
endif

tmp/obj/nonogram_version.h: src/obj/nonogram_version.h.m4 VERSION
	@$(PRINTF) '[version header %s]\n' "$(file <VERSION)"
	@$(MKDIR) '$(@D)'
	@$(M4) -DVERSION='`$(file <VERSION)'"'" < '$<' > '$@'


install:: install-libraries install-headers

tidy::
	-$(FIND) . -name "*~" -exec $(RM) {} \;
	-$(RM) core


# Set this to the comma-separated list of years that should appear in
# the licence.  Do not use characters other than [0-9,] - no spaces.
YEARS=2001,2005-8,2012

update-licence:
	$(FIND) . -name ".svn" -prune -or -type f -print0 | $(XARGS) -0 \
	$(SED) -i 's/Copyright (C) [0-9,-]\+  Steven Simpson/Copyright (C) $(YEARS)  Steven Simpson/g'

distclean:: blank
	$(RM) VERSION
