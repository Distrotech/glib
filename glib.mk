# GLIB - Library of useful C routines

#GTESTER = gtester 			# for non-GLIB packages
#GTESTER_REPORT = gtester-report        # for non-GLIB packages
GTESTER = $(top_builddir)/glib/gtester			# for the GLIB package
GTESTER_REPORT = $(top_builddir)/glib/gtester-report	# for the GLIB package
NULL =

# initialize variables for unconditional += appending
BUILT_SOURCES =
BUILT_EXTRA_DIST =
CLEANFILES = *.log *.trs
DISTCLEANFILES =
MAINTAINERCLEANFILES =
EXTRA_DIST =
TEST_PROGS =
GLIB_GENERATED = 

installed_test_LTLIBRARIES =
installed_test_PROGRAMS =
installed_test_SCRIPTS =
nobase_installed_test_DATA =

noinst_LTLIBRARIES =
noinst_PROGRAMS =
noinst_SCRIPTS =
noinst_DATA =

check_LTLIBRARIES =
check_PROGRAMS =
check_SCRIPTS =
check_DATA =

TESTS =

### testing rules

# test: run all tests in cwd and subdirs
test: test-nonrecursive
if OS_UNIX
	@ for subdir in $(SUBDIRS) . ; do \
	    test "$$subdir" = "." -o "$$subdir" = "po" || \
	    ( cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $? ; \
	  done

# test-nonrecursive: run tests only in cwd
test-nonrecursive: ${TEST_PROGS}
	@test -z "${TEST_PROGS}" || G_TEST_SRCDIR="$(abs_srcdir)" G_TEST_BUILDDIR="$(abs_builddir)" G_DEBUG=gc-friendly MALLOC_CHECK_=2 MALLOC_PERTURB_=$$(($${RANDOM:-256} % 256)) ${GTESTER} --verbose ${TEST_PROGS}
else
test-nonrecursive:
endif

# test-report: run tests in subdirs and generate report
# perf-report: run tests in subdirs with -m perf and generate report
# full-report: like test-report: with -m perf and -m slow
test-report perf-report full-report:	${TEST_PROGS}
	@test -z "${TEST_PROGS}" || { \
	  case $@ in \
	  test-report) test_options="-k";; \
	  perf-report) test_options="-k -m=perf";; \
	  full-report) test_options="-k -m=perf -m=slow";; \
	  esac ; \
	  if test -z "$$GTESTER_LOGDIR" ; then	\
	    G_TEST_SRCDIR="$(abs_srcdir)" G_TEST_BUILDDIR="$(abs_builddir)" ${GTESTER} --verbose $$test_options -o test-report.xml ${TEST_PROGS} ; \
	  elif test -n "${TEST_PROGS}" ; then \
	    G_TEST_SRCDIR="$(abs_srcdir)" G_TEST_BUILDDIR="$(abs_builddir)" ${GTESTER} --verbose $$test_options -o `mktemp "$$GTESTER_LOGDIR/log-XXXXXX"` ${TEST_PROGS} ; \
	  fi ; \
	}
	@ ignore_logdir=true ; \
	  if test -z "$$GTESTER_LOGDIR" ; then \
	    GTESTER_LOGDIR=`mktemp -d "\`pwd\`/.testlogs-XXXXXX"`; export GTESTER_LOGDIR ; \
	    ignore_logdir=false ; \
	  fi ; \
	  if test -d "$(top_srcdir)/.git" ; then \
	    REVISION=`git describe` ; \
	  else \
	    REVISION=$(VERSION) ; \
	  fi ; \
	  for subdir in $(SUBDIRS) . ; do \
	    test "$$subdir" = "." -o "$$subdir" = "po" || \
	    ( cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $? ; \
	  done ; \
	  $$ignore_logdir || { \
	    echo '<?xml version="1.0"?>'              > $@.xml ; \
	    echo '<report-collection>'               >> $@.xml ; \
	    echo '<info>'                            >> $@.xml ; \
	    echo '  <package>$(PACKAGE)</package>'   >> $@.xml ; \
	    echo '  <version>$(VERSION)</version>'   >> $@.xml ; \
	    echo "  <revision>$$REVISION</revision>" >> $@.xml ; \
	    echo '</info>'                           >> $@.xml ; \
	    for lf in `ls -L "$$GTESTER_LOGDIR"/.` ; do \
	      sed '1,1s/^<?xml\b[^>?]*?>//' <"$$GTESTER_LOGDIR"/"$$lf" >> $@.xml ; \
	    done ; \
	    echo >> $@.xml ; \
	    echo '</report-collection>' >> $@.xml ; \
	    rm -rf "$$GTESTER_LOGDIR"/ ; \
	    ${GTESTER_REPORT} --version 2>/dev/null 1>&2 ; test "$$?" != 0 || ${GTESTER_REPORT} $@.xml >$@.html ; \
	  }
.PHONY: test test-report perf-report full-report test-nonrecursive

.PHONY: lcov genlcov lcov-clean
# use recursive makes in order to ignore errors during check
lcov:
	-$(MAKE) $(AM_MAKEFLAGS) -k check
	$(MAKE) $(AM_MAKEFLAGS) genlcov

# we have to massage the lcov.info file slightly to hide the effect of libtool
# placing the objects files in the .libs/ directory separate from the *.c
# we also have to delete tests/.libs/libmoduletestplugin_*.gcda
genlcov:
	rm -f $(top_builddir)/tests/.libs/libmoduletestplugin_*.gcda
	$(LTP) --directory $(top_builddir) --capture --output-file glib-lcov.info --test-name GLIB_PERF --no-checksum --compat-libtool
	LANG=C $(LTP_GENHTML) --prefix $(top_builddir) --output-directory glib-lcov --title "GLib Code Coverage" --legend --show-details glib-lcov.info
	@echo "file://$(abs_top_builddir)/glib-lcov/index.html"

lcov-clean:
	-$(LTP) --directory $(top_builddir) -z
	-rm -rf glib-lcov.info glib-lcov
	-find -name '*.gcda' -print | xargs rm

# run tests in cwd as part of make check
check-local: test-nonrecursive

# We support a fairly large range of possible variables.  It is expected that all types of files in a test suite
# will belong in exactly one of the following variables.
#
# First, we support the usual automake suffixes, but in lowercase, with the customary meaning:
#
#   test_programs, test_scripts, test_data, test_ltlibraries
#
# The above are used to list files that are involved in both uninstalled and installed testing.  The
# test_programs and test_scripts are taken to be actual testcases and will be run as part of the test suite.
# Note that _data is always used with the nobase_ automake variable name to ensure that installed test data is
# installed in the same way as it appears in the package layout.
#
# In order to mark a particular file as being only for one type of testing, use 'installed' or 'uninstalled',
# like so:
#
#   installed_test_programs, uninstalled_test_programs
#   installed_test_scripts, uninstalled_test_scripts
#   installed_test_data, uninstalled_test_data
#   installed_test_ltlibraries, uninstalled_test_ltlibraries
#
# Additionally, we support 'extra' infixes for programs and scripts.  This is used for support programs/scripts
# that should not themselves be run as testcases (but exist to be used from other testcases):
#
#   test_extra_programs, installed_test_extra_programs, uninstalled_test_extra_programs
#   test_extra_scripts, installed_test_extra_scripts, uninstalled_test_extra_scripts
#
# Additionally, for _scripts and _data, we support the customary dist_ prefix so that the named script or data
# file automatically end up in the tarball.
#
#   dist_test_scripts, dist_test_data, dist_test_extra_scripts
#   dist_installed_test_scripts, dist_installed_test_data, dist_installed_test_extra_scripts
#   dist_uninstalled_test_scripts, dist_uninstalled_test_data, dist_uninstalled_test_extra_scripts
#
# Note that no file is automatically disted unless it appears in one of the dist_ variables.  This follows the
# standard automake convention of not disting programs scripts or data by default.
#
# test_programs, test_scripts, uninstalled_test_programs and uninstalled_test_scripts (as well as their disted
# variants) will be run as part of the in-tree 'make check'.  These are all assumed to be runnable under
# gtester.  That's a bit strange for scripts, but it's possible.

# we use test -z "$(TEST_PROGS)" above, so make sure we have no extra whitespace...
TEST_PROGS += $(strip $(test_programs) $(test_scripts) $(uninstalled_test_programs) $(uninstalled_test_scripts) \
                      $(dist_test_scripts) $(dist_uninstalled_test_scripts))

if OS_WIN32
TESTS += $(test_programs) $(test_scripts) $(uninstalled_test_programs) $(uninstalled_test_scripts) \
         $(dist_test_scripts) $(dist_uninstalled_test_scripts)
endif

# Note: build even the installed-only targets during 'make check' to ensure that they still work.
# We need to do a bit of trickery here and manage disting via EXTRA_DIST instead of using dist_ prefixes to
# prevent automake from mistreating gmake functions like $(wildcard ...) and $(addprefix ...) as if they were
# filenames, including removing duplicate instances of the opening part before the space, eg. '$(addprefix'.
all_test_programs     = $(test_programs) $(uninstalled_test_programs) $(installed_test_programs) \
                        $(test_extra_programs) $(uninstalled_test_extra_programs) $(installed_test_extra_programs)
all_test_scripts      = $(test_scripts) $(uninstalled_test_scripts) $(installed_test_scripts) \
                        $(test_extra_scripts) $(uninstalled_test_extra_scripts) $(installed_test_extra_scripts)
all_dist_test_scripts = $(dist_test_scripts) $(dist_uninstalled_test_scripts) $(dist_installed_test_scripts) \
                        $(dist_test_extra_scripts) $(dist_uninstalled_test_extra_scripts) $(dist_installed_test_extra_scripts)
all_test_scripts     += $(all_dist_test_scripts)
EXTRA_DIST           += $(all_dist_test_scripts)
all_test_data         = $(test_data) $(uninstalled_test_data) $(installed_test_data)
all_dist_test_data    = $(dist_test_data) $(dist_uninstalled_test_data) $(dist_installed_test_data)
all_test_data        += $(all_dist_test_data)
EXTRA_DIST           += $(all_dist_test_data)
all_test_ltlibs       = $(test_ltlibraries) $(uninstalled_test_ltlibraries) $(installed_test_ltlibraries)

if ENABLE_ALWAYS_BUILD_TESTS
noinst_LTLIBRARIES += $(all_test_ltlibs)
noinst_PROGRAMS += $(all_test_programs)
noinst_SCRIPTS += $(all_test_scripts)
noinst_DATA += $(all_test_data)
else
check_LTLIBRARIES += $(all_test_ltlibs)
check_PROGRAMS += $(all_test_programs)
check_SCRIPTS += $(all_test_scripts)
check_DATA += $(all_test_data)
endif

if ENABLE_INSTALLED_TESTS
installed_test_PROGRAMS += $(test_programs) $(installed_test_programs) \
                          $(test_extra_programs) $(installed_test_extra_programs)
installed_test_SCRIPTS += $(test_scripts) $(installed_test_scripts) \
                          $(test_extra_scripts) $(test_installed_extra_scripts)
installed_test_SCRIPTS += $(dist_test_scripts) $(dist_test_extra_scripts) \
                          $(dist_installed_test_scripts) $(dist_installed_test_extra_scripts)
nobase_installed_test_DATA += $(test_data) $(installed_test_data)
nobase_installed_test_DATA += $(dist_test_data) $(dist_installed_test_data)
installed_test_LTLIBRARIES += $(test_ltlibraries) $(installed_test_ltlibraries)
installed_testcases = $(test_programs) $(installed_test_programs) \
                      $(test_scripts) $(installed_test_scripts) \
                      $(dist_test_scripts) $(dist_installed_test_scripts)

installed_test_meta_DATA = $(installed_testcases:=.test)

%.test: %$(EXEEXT) Makefile
	$(AM_V_GEN) (echo '[Test]' > $@.tmp; \
	echo 'Type=session' >> $@.tmp; \
	echo 'Exec=$(installed_testdir)/$<' >> $@.tmp; \
	mv $@.tmp $@)

CLEANFILES += $(installed_test_meta_DATA)
endif


# This is like AM_V_GEN, except that it strips ".stamp" from its output
_GLIB_V_GEN = $(_glib_v_gen_$(V))
_glib_v_gen_ = $(_glib_v_gen_$(AM_DEFAULT_VERBOSITY))
_glib_v_gen_0 = @echo "  GEN     " $(subst .stamp,,$@);

# _glib_all_sources contains every file that is (directly or
# indirectly) part of any _SOURCES variable in Makefile.am. We use
# this to find the files we need to generate rules for. (We can't just
# use '%' rules to build things because then the .stamp files get
# treated as "intermediate files" by make, and then things don't
# always get rebuilt when we need them to be.)
#
# ($(sort) is used here only for its side effect of removing
# duplicates.)
_glib_all_sources = $(sort $(foreach var,$(filter %_SOURCES,$(.VARIABLES)),$($(var))))

# We can't add our generated files to BUILT_SOURCES because that would
# create recursion with _glib_all_sources
_glib_built_sources =
all: $(_glib_built_sources)
check: $(_glib_built_sources)
install: $(_glib_built_sources)


# glib-mkenums support
#
# glib.mk will automatically build/update "foo-enum-types.c" and
# "foo-enum-types.h" (or "fooenumtypes.c" and "fooenumtypes.h") as
# needed, if there is an appropriate $(foo_enum_types_sources) /
# $(fooenumtypes_sources) variable indicating the source files to use.
#
# For your convenience, any .c files or glib.mk-generated sources in
# the _sources variable will be ignored. This means you can usually
# just set it to the value of your library/program's _HEADERS and/or
# _SOURCES variables, even if that variable contains the files being
# generated.
#
# You can set GLIB_MKENUMS_H_FLAGS and GLIB_MKENUMS_C_FLAGS (or an
# appropriate file-specific variable, eg
# foo_enum_types_MKENUMS_H_FLAGS) to set/override certain glib-mkenums
# options. In particular, you can do:
#
#     GLIB_MKENUMS_C_FLAGS = --fhead "\#define FOO_I_KNOW_THIS_IS_UNSTABLE"
#
# (The backslash is necessary to keep make from thinking the "#" is
# the start of a comment.)
#
# You are responsible for adding the generated .c and .h files to
# either CLEANFILES, or to DISTFILES and DISTCLEANFILES, as
# appropriate. glib.mk will ensure that the .stamp files it builds get
# cleaned/disted along with the generated .c and .h files.
#
# You do not need to add the generated files to BUILT_SOURCES; glib.mk
# will cause them to be built at the correct time (but note that it does
# not actually add them to BUILT_SOURCES).


# These are used as macros (with the value of $(1) possibly inherited
# from the "caller")
#   _glib_enum_types_prefix("foo-enum-types") = "foo_enum_types"
#   _glib_enum_types_guard("foo-enum-types") = "__FOO_ENUM_TYPES_H__"
#   _glib_enum_types_sources_var("foo-enum_types") = "foo_enum_types_sources"
#   _glib_enum_types_sources = the filtered value of $(foo_enum_types_sources)
#   _glib_enum_types_h_sources = the .h files in $(_glib_enum_types_sources)
_glib_enum_types_prefix = $(subst -,_,$(notdir $(1)))
_glib_enum_types_guard = __$(shell LC_ALL=C echo $(_glib_enum_types_prefix) | tr 'a-z' 'A-Z')_H__
_glib_enum_types_sources_var = $(_glib_enum_types_prefix)_sources
_glib_enum_types_sources = $(filter-out $(_glib_built_sources),$($(_glib_enum_types_sources_var)))
_glib_enum_types_h_sources = $(filter %.h,$(_glib_enum_types_sources))

# _glib_all_enum_types contains the basenames (eg, "fooenumtypes",
# "bar-enum-types") of all enum-types files known to the Makefile.
# _glib_generated_enum_types contains only the ones being generated by
# glib.mk.
_glib_all_enum_types = $(subst .h,,$(notdir $(filter %enum-types.h %enumtypes.h,$(_glib_all_sources))))
_glib_generated_enum_types = $(foreach f,$(_glib_all_enum_types),$(if $(strip $(call _glib_enum_types_sources,$f)),$f))

# _glib_make_mkenums_rules is a multi-line macro that outputs a set of
# rules for a single .h/.c pair (whose basename is $(1)). automake
# doesn't recognize GNU make's define/endef syntax, so if we defined
# the macro directly, it would try to, eg, add the literal "$(1).h" to
# _glib_built_sources. So we hide the macro by prefixing each line
# with ":::", and then use $(subst) to extract the actual rule.

# We have to include "Makefile" in the dependencies so that the
# outputs get regenerated when you remove files from
# foo_enum_types_sources. (This is especially important for
# foo-enum-types.h, which might otherwise try to #include files that
# no longer exist.).

define _glib_make_mkenums_rules_hidden
:::$(1).h.stamp: $(_glib_enum_types_h_sources) Makefile
:::	$$(_GLIB_V_GEN) $$(GLIB_MKENUMS) \
:::		--fhead "/* Generated by glib-mkenums. Do not edit */\n\n" \
:::		--fhead "#ifndef $(_glib_enum_types_guard)\n" \
:::		--fehad "#define $(_glib_enum_types_guard)\n\n" \
:::		$$(GLIB_MKENUMS_H_FLAGS) \
:::		$$($(_glib_enum_types_prefix)_MKENUMS_H_FLAGS) \
:::		--fhead "#include <glib-object.h>\n\n" \
:::		--fhead "G_BEGIN_DECLS\n" \
:::		--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n" \
:::		--vhead "#define @ENUMPREFIX@_TYPE_@ENUMSHORT@ (@enum_name@_get_type ())\n" \
:::		--ftail "G_END_DECLS\n\n#endif /* $(_glib_enum_types_guard) */" \
:::		$$(filter-out Makefile, $$^) > $(1).h.tmp && \
:::	(cmp -s $(1).h.tmp $(1).h || cp $(1).h.tmp $(1).h) && \
:::	rm -f $(1).h.tmp && \
:::	echo timestamp > $$@
:::
:::$(1).h: $(1).h.stamp
:::	@true
:::
:::$(1).c.stamp: $(_glib_enum_types_h_sources) Makefile
:::	$$(_GLIB_V_GEN) $$(GLIB_MKENUMS) \
:::		--fhead "/* Generated by glib-mkenums. Do not edit */\n\n" \
:::		--fhead "#ifdef HAVE_CONFIG_H\n" \
:::		--fhead "#include \"config.h\"\n" \
:::		--fhead "#endif\n\n" \
:::		--fhead "#include \"$(notdir $(1)).h\"\n" \
:::		$$(GLIB_MKENUMS_C_FLAGS) \
:::		$$($(_glib_enum_types_prefix)_MKENUMS_C_FLAGS) \
:::		--fhead "$$(foreach f,$$(filter-out Makefile,$$(^F)),\n#include \"$$(f)\")\n\n" \
:::		--vhead "GType\n" \
:::		--vhead "@enum_name@_get_type (void)\n" \
:::		--vhead "{\n" \
:::		--vhead "  static volatile gsize g_define_type_id__volatile = 0;\n\n" \
:::		--vhead "  if (g_once_init_enter (&g_define_type_id__volatile))\n" \
:::		--vhead "    {\n" \
:::		--vhead "      static const G@Type@Value values[] = {\n" \
:::		--vprod "        { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" },\n" \
:::		--vtail "        { 0, NULL, NULL }\n" \
:::		--vtail "      };\n" \
:::		--vtail "      GType g_define_type_id =\n" \
:::		--vtail "        g_@type@_register_static (g_intern_static_string (\"@EnumName@\"), values);\n" \
:::		--vtail "      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);\n" \
:::		--vtail "    }\n\n" \
:::		--vtail "  return g_define_type_id__volatile;\n" \
:::		--vtail "}\n" \
:::		$$(filter-out Makefile, $$^) > $(1).c.tmp && \
:::	(cmp -s $(1).c.tmp $(1).c || cp $(1).c.tmp $(1).c) && \
:::	rm -f $(1).c.tmp && \
:::	echo timestamp > $$@
:::
:::$(1).c: $(1).c.stamp
:::	@true
:::
:::_glib_built_sources += $(1).h $(1).c
endef
_glib_make_mkenums_rules = $(subst :::,,$(_glib_make_mkenums_rules_hidden))

# Run _glib_make_mkenums_rules for each set of generated files
$(foreach f,$(_glib_generated_enum_types),$(eval $(call _glib_make_mkenums_rules,$f)))

# clean/dist stamps when cleaning/disting generated files
_glib_enumtypes_filter = $(filter %enumtypes.h %enumtypes.c %-enum-types.h %-enum-types.c,$(1))

clean-am: glib-mkenums-clean
glib-mkenums-clean:
       @$(if $(strip $(call _glib_enumtypes_filter,$(CLEANFILES))),rm -f $(foreach f,$(call _glib_enumtypes_filter,$(CLEANFILES)),$(f).stamp),:)

distclean-am: glib-mkenums-distclean
glib-mkenums-distclean:
       @$(if $(strip $(call _glib_enumtypes_filter,$(DISTCLEANFILES))),rm -f $(foreach f,$(call _glib_enumtypes_filter,$(DISTCLEANFILES)),$(f).stamp),:)

dist-hook: glib-mkenums-dist-hook
glib-mkenums-dist-hook:
       @$(if $(strip $(call _glib_enumtypes_filter,$(DISTFILES))),cp -p $(foreach f,$(call _glib_enumtypes_filter,$(DISTFILES)),$(f).stamp) $(distdir)/,:)



# glib-genmarshal support
#
# glib.mk will automatically build/update "foo-marshal.c" and
# "foo-marshal.h" (or "foomarshal.c" and "foomarshal.h") as needed, if
# there is an appropriate $(foo_marshal_sources) /
# $(foomarshal_sources) variable indicating the source files to use;
# glib.mk will generate a "foo-marshal.list" file containing all
# _foo_marshal_* functions referenced by $(foo_marshal_sources), and
# will then rebuild the generated C files whenever the list changes.
#
# For your convenience, any .h files or glib.mk-generated files in
# $(foo_marshal_sources) will be ignored. This means you can usually
# just set foo_marshal_sources to the value of your library/program's
# _SOURCES variable, even if that variable contains foo-marshal.c.
#
# You can set GLIB_GENMARSHAL_H_FLAGS and GLIB_GENMARSHAL_C_FLAGS (or
# an appropriate file-specific variable, eg
# foo_marshal_GENMARSHAL_H_FLAGS) to set/override glib-genmarshal
# options.

# see the comments in the glib-mkenums section for details of how this all works

_glib_marshal_prefix = $(subst marshal,,$(subst _marshal,,$(subst -,_,$(notdir $(1)))))_marshal
_glib_marshal_sources_var = $(_glib_marshal_prefix)_sources
_glib_marshal_sources = $(filter-out $(_glib_built_sources),$($(_glib_marshal_sources_var)))
_glib_marshal_c_sources = $(filter %.c,$(_glib_marshal_sources))

_glib_all_marshal = $(subst .h,,$(notdir $(filter %marshal.h,$(_glib_all_sources))))
_glib_generated_marshal = $(foreach f,$(_glib_all_marshal),$(if $(strip $(call _glib_marshal_sources,$f)),$f))

define _glib_make_genmarshal_rules_hidden
:::$(1).list.stamp: $(_glib_marshal_c_sources) Makefile
:::	$$(_GLIB_V_GEN) LC_ALL=C sed -ne 's/.*_$(_glib_marshal_prefix)_\([_A-Z]*\).*/\1/p' $$(filter-out Makefile, $$^) | sort -u | sed -e 's/__/:/' -e 's/_/,/g' > $(1).list.tmp && \
:::	(cmp -s $(1).list.tmp $(1).list || cp $(1).list.tmp $(1).list) && \
:::	rm -f $(1).list.tmp && \
:::	echo timestamp > $$@
:::
:::$(1).list: $(1).list.stamp
:::	@true
:::
:::$(1).h: $(1).list
:::	$$(_GLIB_V_GEN) $$(GLIB_GENMARSHAL) \
:::		--prefix=_$(_glib_marshal_prefix) --header \
:::		$$(GLIB_GENMARSHAL_H_FLAGS) \
:::		$$($(_glib_marshal_prefix)_GENMARSHAL_H_FLAGS) \
:::		$$< > $$@.tmp && \
:::	mv $$@.tmp $$@
:::
:::$(1).c: $(1).list
:::	$$(_GLIB_V_GEN) (echo "#include \"$$(subst .c,.h,$$(@F))\""; $$(GLIB_GENMARSHAL) \
:::		--prefix=_$(_glib_marshal_prefix) --body \
:::		$$(GLIB_GENMARSHAL_C_FLAGS) \
:::		$$($(_glib_marshal_prefix)_GENMARSHAL_C_FLAGS) \
:::		$$< ) > $$@.tmp && \
:::	mv $$@.tmp $$@
:::
:::CLEANFILES += $(1).list.stamp $(1).list
:::DISTCLEANFILES += $(1).h $(1).c
endef
_glib_make_genmarshal_rules = $(subst :::,,$(_glib_make_genmarshal_rules_hidden))

$(foreach f,$(_glib_generated_marshal),$(eval $(call _glib_make_genmarshal_rules,$f)))


# glib-compile-schemas support
#
# Any foo.gschemas.xml files listed in gsettingsschema_DATA will be
# validated before installation, and (if --disable-schemas-compile was
# not passed to configure) compiled after installation.
#
# glib.mk will automatically build/update any "org.foo.bar.enums.xml"
# files in gsettingsschema_DATA, if there is an appropriate
# $(org_foo_bar_enums_xml_sources) variable indicating the source
# files to use. All enums files will automatically be built before any
# schema files are validated.

# see the comments in the glib-mkenums section for details of how this all works
_GLIB_ENUMS_XML_GENERATED = $(filter %.enums.xml,$(gsettingsscheme_DATA))
_GLIB_GSETTINGS_SCHEMA_FILES = $(filter %.gschema.xml,$(gsettingsschema_DATA))
_GLIB_GSETTINGS_VALID_FILES = $(subst .xml,.valid,$(_GLIB_GSETTINGS_SCHEMA_FILES))

_glib_enums_xml_prefix = $(subst .,_,$(notdir $(1)))
_glib_enums_xml_sources_var = $(_glib_enums_xml_prefix)_sources
_glib_enums_xml_sources = $(filter-out $(_glib_built_sources),$($(_glib_enums_xml_sources_var)))
_glib_enums_xml_namespace = $(subst .enums.xml,,$(notdir $(1)))

define _glib_make_enums_xml_rule_hidden
:::$(1): $(_glib_enums_xml_sources) Makefile
:::	$$(_GLIB_V_GEN) $$(GLIB_MKENUMS) \
:::		--comments '<!-- @comment@ -->' \
:::		--fhead "<schemalist>" \
:::		--vhead "  <@type@ id='$(_glib_enums_xml_namespace).@EnumName@'>" \
:::		--vprod "    <value nick='@valuenick@' value='@valuenum@'/>" \
:::		--vtail "  </@type@>" \
:::		--ftail "</schemalist>" \
:::		$$(filter-out Makefile, $$^) > $$@.tmp && \
:::	mv $$@.tmp $$@
endef
_glib_make_enums_xml_rule = $(subst :::,,$(_glib_make_enums_xml_rule_hidden))

_GLIB_V_CHECK = $(_glib_v_check_$(V))
_glib_v_check_ = $(_glib_v_check_$(AM_DEFAULT_VERBOSITY))
_glib_v_check_0 = @echo "  CHECK   " $(subst .valid,.xml,$@);

define _glib_make_schema_validate_rule_hidden
:::$(subst .xml,.valid,$(1)): $(_GLIB_ENUMS_XML_GENERATED) $(1)
:::	$$(_GLIB_V_CHECK) $$(GLIB_COMPILE_SCHEMAS) --strict --dry-run $$(addprefix --schema-file=,$$^) && touch $$@
endef
_glib_make_schema_validate_rule = $(subst :::,,$(_glib_make_schema_validate_rule_hidden))

define _glib_make_schema_rules_hidden
:::all-am: $(_GLIB_GSETTINGS_VALID_FILES)
:::
:::install-data-am: glib-install-schemas-hook
:::
:::glib-install-schemas-hook: install-gsettingsschemaDATA
:::	@test -n "$(GSETTINGS_DISABLE_SCHEMAS_COMPILE)$(DESTDIR)" || (echo $(GLIB_COMPILE_SCHEMAS) $(gsettingsschemadir); $(GLIB_COMPILE_SCHEMAS) $(gsettingsschemadir))
:::
:::uninstall-am: glib-uninstall-schemas-hook
:::
:::glib-uninstall-schemas-hook: uninstall-gsettingsschemaDATA
:::	@test -n "$(GSETTINGS_DISABLE_SCHEMAS_COMPILE)$(DESTDIR)" || (echo $(GLIB_COMPILE_SCHEMAS) $(gsettingsschemadir); $(GLIB_COMPILE_SCHEMAS) $(gsettingsschemadir))
:::
:::.PHONY: glib-install-schemas-hook glib-uninstall-schemas-hook
endef
_glib_make_schema_rules = $(subst :::,,$(_glib_make_schema_rules_hidden))

CLEANFILES += $(_GLIB_ENUMS_XML_GENERATED) $(_GLIB_GSETTINGS_VALID_FILES)

$(foreach f,$(_GLIB_ENUMS_XML_GENERATED),$(eval $(call _glib_make_enums_xml_rule,$f)))
$(foreach f,$(_GLIB_GSETTINGS_SCHEMA_FILES),$(eval $(call _glib_make_schema_validate_rule,$f)))
$(if $(_GLIB_GSETTINGS_SCHEMA_FILES),$(eval $(_glib_make_schema_rules)))


# glib-compile-resources support
#
# To compile resources, add files with names ending in
# "resources.h" and "resources.c" to GLIB_GENERATED:
#
#    GLIB_GENERATED += foo-resources.h foo-resources.c
#
# glib.mk will then compile "foo.gresource.xml" into the named files,
# creating a method foo_get_resource(). glib.mk will figure out the
# resource's dependencies automatically, and add the dependency files
# to EXTRA_DIST.
#
# If you specify both a .c and a .h file, glib.mk will compile the
# resource with the --manual-register flag. If you specify only a .c
# file, it will not.
#
# Alternatively, you can build a standalone resource file by doing:
#
#    GLIB_GENERATED += foo.gresource
#
# You can set GLIB_COMPILE_RESOURCES_FLAGS (or an appropriate
# file-specific variable, eg foo_resources_COMPILE_RESOURCES_FLAGS or
# foo_gresource_COMPILE_RESOURCES_FLAGS) to set/override
# glib-compile-resources options.

_GLIB_RESOURCES_CH_GENERATED = $(subst .h,,$(filter %resources.h,$(GLIB_GENERATED)))
_GLIB_RESOURCES_C_GENERATED = $(filter-out $(_GLIB_RESOURCES_CH_GENERATED),$(subst .c,,$(filter %resources.c,$(GLIB_GENERATED))))

_glib_resources_c_prefix = $(subst resources,,$(subst _resources,,$(subst -,_,$(notdir $(1)))))
_glib_resources_c_xml = $(subst resources,,$(subst -resources,,$(1))).gresource.xml
_glib_resources_c_sources = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(_glib_resources_c_xml))

define _glib_make_h_resources_rules_hidden
:::$(1).h: $(_glib_resources_c_xml) $(_glib_resources_c_sources)
:::	$$(_GLIB_V_GEN) $$(GLIB_COMPILE_RESOURCES) \
:::		--target=$$@ --sourcedir="$(srcdir)" \
:::		--generate-header $(2) \
:::		--c-name $(_glib_resources_c_prefix) \
:::		$$(GLIB_COMPILE_RESOURCES_FLAGS) \
:::		$$($(_glib_resources_c_prefix)_resources_COMPILE_RESOURCES_FLAGS) \
:::		$$<
:::
:::DISTCLEANFILES += $(1).h
endef
_glib_make_h_resources_rules = $(subst :::,,$(_glib_make_h_resources_rules_hidden))

define _glib_make_c_resources_rules_hidden
:::$(1).c: $(_glib_resources_c_xml) $(_glib_resources_c_sources)
:::	$$(_GLIB_V_GEN) $$(GLIB_COMPILE_RESOURCES) \
:::		--target=$$@ --sourcedir="$(srcdir)" \
:::		--generate-source $(2) \
:::		--c-name $(_glib_resources_c_prefix) \
:::		$$(GLIB_COMPILE_RESOURCES_FLAGS) \
:::		$$($(_glib_resources_c_prefix)_resources_COMPILE_RESOURCES_FLAGS) \
:::		$$<
:::
:::DISTCLEANFILES += $(1).c
:::EXTRA_DIST += $(_glib_resources_c_xml) $(_glib_resources_c_sources)
endef
_glib_make_c_resources_rules = $(subst :::,,$(_glib_make_c_resources_rules_hidden))

$(foreach f,$(_GLIB_RESOURCES_CH_GENERATED),$(eval $(call _glib_make_h_resources_rules,$f,--manual-register)))
$(foreach f,$(_GLIB_RESOURCES_CH_GENERATED),$(eval $(call _glib_make_c_resources_rules,$f,--manual-register)))
$(foreach f,$(_GLIB_RESOURCES_C_GENERATED),$(eval $(call _glib_make_c_resources_rules,$f,)))

_GLIB_RESOURCES_STANDALONE_GENERATED = $(subst .gresource,,$(filter %.gresource,$(GLIB_GENERATED)))

_glib_resources_standalone_prefix = $(subst -,_,$(notdir $(1)))
_glib_resources_standalone_xml = $(1).gresource.xml
_glib_resources_standalone_sources = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(_glib_resources_standalone_xml))

define _glib_make_standalone_resources_rules_hidden
:::$(1).gresource: $(_glib_resources_standalone_xml) $(_glib_resources_standalone_sources)
:::	$$(_GLIB_V_GEN) $$(GLIB_COMPILE_RESOURCES) \
:::		--target=$$@ --sourcedir="$(srcdir)" \
:::		$$(GLIB_COMPILE_RESOURCES_FLAGS) \
:::		$$($(_glib_resources_standalone_prefix)_COMPILE_RESOURCES_FLAGS) \
:::		$$<
:::
:::CLEANFILES += $(1).gresource
:::EXTRA_DIST += $(_glib_resources_standalone_xml) $(_glib_resources_standalone_sources)
endef
_glib_make_standalone_resources_rules = $(subst :::,,$(_glib_make_standalone_resources_rules_hidden))

$(foreach f,$(_GLIB_RESOURCES_STANDALONE_GENERATED),$(eval $(call _glib_make_standalone_resources_rules,$f)))
