# For build target -----------------------------------------------------------|
LIBS=`pkg-config --libs glib-2.0 libusb`
INCS=`pkg-config --cflags glib-2.0 libusb`

# For install target ---------------------------------------------------------|
# App to run to install files, usually in path; set otherwise
INSTALL=install

# FW data
FIRMWARE_NAMESPEC=r5u87x-%vid%-%pid%.fw
FIRMWARE=`ls ucode | xargs`

# Install names
LOADER_INSTALL=r5u87x-loader
UDEV_INSTALL=/etc/udev/rules.d/

# Directories
PREFIX=/usr
INSTALL_PATH=$(DESTDIR)$(PREFIX)
sbindir=/sbin
libdir=/lib
firmdir=$(libdir)/r5u87x/ucode
UCODE_PATH=$(PREFIX)$(firmdir)/$(FIRMWARE_NAMESPEC)

# For rules and make targets -------------------------------------------------|
RULESFILE=contrib/90-r5u87x-loader.rules

# Automake targets -----------------------------------------------------------|

.c.o:
	$(CC) -g -Wall -DHAVE_CONFIG_H -DUCODE_PATH=\"$(UCODE_PATH)\" $(CFLAGS) $(INCS) -c $*.c $*.h

all: loader rules

loader: loader.o
	$(CC) -g -Wall $(CFLAGS) -o $@ loader.o $(LIBS)
	
fw-extract:
	$(CC) -g -Wall -DDEBUG $(CFLAGS) -o $@ fw-extract.c

clean:
	rm -fr *.o loader
	rm -fr *.gch loader
	if [ -f $(RULESFILE) ]; then \
	    rm -f $(RULESFILE); \
    fi

install: all
	$(INSTALL) -d $(INSTALL_PATH)$(bindir)
	$(INSTALL) -d $(INSTALL_PATH)$(sbindir)
	$(INSTALL) -m 0755 loader $(INSTALL_PATH)$(sbindir)/$(LOADER_INSTALL)
	$(INSTALL) -d $(INSTALL_PATH)$(firmdir)
	@for fw in $(FIRMWARE); do \
		echo "$(INSTALL) -m 0644 ucode/$$fw $(INSTALL_PATH)$(firmdir)/$$fw" ; \
		$(INSTALL) -m 0644 ucode/$$fw $(INSTALL_PATH)$(firmdir)/$$fw || exit 1 ; \
	done
	
	## If we have the rules file generated, install it while we're here
	if [ -f $(RULESFILE) ]; then \
		$(INSTALL) -d $(DESTDIR)$(UDEV_INSTALL); \
		$(INSTALL) -m 0644 $(RULESFILE) $(DESTDIR)$(UDEV_INSTALL); \
	fi

uninstall:
	rm -fv $(INSTALL_PATH)$(sbindir)/$(LOADER_INSTALL)
	rm -rfv $(INSTALL_PATH)$(libdir)/r5u87x

$(RULESFILE):
	# extract preamble from template
	cat $(RULESFILE).in | \
		awk 'BEGIN{P=1;}/^###BEGINTEMPLATE###/{P=0;} {if (P) print;}' \
		| grep -v '^###' >$@
	# process template for each firmware file
	# loader is part of regexp REPL_LOADER has escaped slashes (to work for simple paths)
	for sedline in `ls ucode | sed 's/^r5u87x-\([0-9a-zA-Z]\+\)-\([0-9a-zA-Z]\+\)\.fw$$/s\/#VENDORID#\/\1\/g;s\/#PRODUCTID#\/\2\/g/p;d'`; do \
		REPL_LOADER=$$(echo "$(PREFIX)$(sbindir)/$(LOADER_INSTALL)" | sed 's/\//\\\//g'); \
		cat $(RULESFILE).in | \
			awk 'BEGIN{P=0;}/^###BEGINTEMPLATE###/{P=1;}/^###ENDTEMPLATE###/{P=0;} {if (P) print;}' | \
			grep -v '^###' | \
			sed -e "$$sedline" \
			    -e "s/#LOADER#/$$REPL_LOADER/g" >>$@; \
		done >>$@
	# extract postscript from template
	cat $(RULESFILE).in | \
		awk 'BEGIN{P=0;}/^###ENDTEMPLATE###/{P=1;} {if (P) print;}' \
		| grep -v '^###' >>$@

rules: $(RULESFILE)
