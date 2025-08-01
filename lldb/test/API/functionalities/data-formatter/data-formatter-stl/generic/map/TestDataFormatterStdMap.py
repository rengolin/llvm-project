"""
Test lldb data formatter subsystem.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class StdMapDataFormatterTestCase(TestBase):
    def setUp(self):
        TestBase.setUp(self)
        ns = "ndk" if lldbplatformutil.target_is_android() else ""
        self.namespace = "std"

    def check_pair(self, first_value, second_value):
        pair_children = [
            ValueCheck(name="first", value=first_value),
            ValueCheck(name="second", value=second_value),
        ]
        return ValueCheck(children=pair_children)

    def do_test(self, *, supports_end_iter=False):
        """Test that that file and class static variables display correctly."""
        self.runCmd("file " + self.getBuildArtifact("a.out"), CURRENT_EXECUTABLE_SET)

        bkpt = self.target().FindBreakpointByID(
            lldbutil.run_break_set_by_source_regexp(
                self, "Set break point at this line."
            )
        )

        self.runCmd("run", RUN_SUCCEEDED)

        # The stop reason of the thread should be breakpoint.
        self.expect(
            "thread list",
            STOPPED_DUE_TO_BREAKPOINT,
            substrs=["stopped", "stop reason = breakpoint"],
        )

        # This is the function to remove the custom formats in order to have a
        # clean slate for the next test case.
        def cleanup():
            self.runCmd("type format clear", check=False)
            self.runCmd("type summary clear", check=False)
            self.runCmd("type filter clear", check=False)
            self.runCmd("type synth clear", check=False)

        # Execute the cleanup function during test case tear down.
        self.addTearDownHook(cleanup)

        ns = self.namespace
        self.expect_expr("ii", result_summary="size=0", result_children=[])

        self.expect("frame var ii", substrs=["%s::map" % ns, "size=0", "{}"])

        lldbutil.continue_to_breakpoint(self.process(), bkpt)

        self.expect_expr(
            "ii",
            result_summary="size=2",
            result_children=[self.check_pair("0", "0"), self.check_pair("1", "1")],
        )

        self.expect(
            "frame variable ii",
            substrs=[
                "%s::map" % ns,
                "size=2",
                "[0] = ",
                "first = 0",
                "second = 0",
                "[1] = ",
                "first = 1",
                "second = 1",
            ],
        )

        lldbutil.continue_to_breakpoint(self.process(), bkpt)

        self.expect(
            "frame variable ii",
            substrs=[
                "%s::map" % ns,
                "size=4",
                "[2] = ",
                "first = 2",
                "second = 0",
                "[3] = ",
                "first = 3",
                "second = 1",
            ],
        )

        lldbutil.continue_to_breakpoint(self.process(), bkpt)

        self.expect(
            "expression ii",
            substrs=[
                "%s::map" % ns,
                "size=8",
                "[5] = ",
                "first = 5",
                "second = 0",
                "[7] = ",
                "first = 7",
                "second = 1",
            ],
        )

        self.expect(
            "frame var ii",
            substrs=[
                "%s::map" % ns,
                "size=8",
                "[5] = ",
                "first = 5",
                "second = 0",
                "[7] = ",
                "first = 7",
                "second = 1",
            ],
        )

        # check access-by-index
        self.expect("frame variable ii[0]", substrs=["first = 0", "second = 0"])
        self.expect("frame variable ii[3]", substrs=["first =", "second ="])

        # (Non-)const key/val iterators
        self.expect_expr(
            "it",
            result_children=[
                ValueCheck(name="first", value="0"),
                ValueCheck(name="second", value="0"),
            ],
        )
        self.expect_expr(
            "const_it",
            result_children=[
                ValueCheck(name="first", value="0"),
                ValueCheck(name="second", value="0"),
            ],
        )
        if supports_end_iter:
            self.expect("frame variable it_end", substrs=["= end"])
            self.expect("frame variable const_it_end", substrs=["= end"])

        # check that MightHaveChildren() gets it right
        self.assertTrue(
            self.frame().FindVariable("ii").MightHaveChildren(),
            "ii.MightHaveChildren() says False for non empty!",
        )

        # check that the expression parser does not make use of
        # synthetic children instead of running code
        # TOT clang has a fix for this, which makes the expression command here succeed
        # since this would make the test fail or succeed depending on clang version in use
        # this is safer commented for the time being
        # self.expect("expression ii[8]", matching=False, error=True,
        #            substrs = ['1234567'])

        self.runCmd("continue")

        self.expect("frame variable ii", substrs=["%s::map" % ns, "size=0", "{}"])

        self.expect("frame variable si", substrs=["%s::map" % ns, "size=0", "{}"])

        self.runCmd("continue")

        self.expect(
            "frame variable si",
            substrs=[
                "%s::map" % ns,
                "size=1",
                "[0] = ",
                'first = "zero"',
                "second = 0",
            ],
        )

        lldbutil.continue_to_breakpoint(self.process(), bkpt)

        self.expect(
            "frame variable si",
            substrs=[
                "%s::map" % ns,
                "size=4",
                '[0] = (first = "one", second = 1)',
                '[1] = (first = "three", second = 3)',
                '[2] = (first = "two", second = 2)',
                '[3] = (first = "zero", second = 0)',
            ],
        )

        self.expect(
            "expression si",
            substrs=[
                "%s::map" % ns,
                "size=4",
                '[0] = (first = "one", second = 1)',
                '[1] = (first = "three", second = 3)',
                '[2] = (first = "two", second = 2)',
                '[3] = (first = "zero", second = 0)',
            ],
        )

        # check that MightHaveChildren() gets it right
        self.assertTrue(
            self.frame().FindVariable("si").MightHaveChildren(),
            "si.MightHaveChildren() says False for non empty!",
        )

        # check access-by-index
        self.expect("frame variable si[0]", substrs=["first = ", "one", "second = 1"])

        # check that the expression parser does not make use of
        # synthetic children instead of running code
        # TOT clang has a fix for this, which makes the expression command here succeed
        # since this would make the test fail or succeed depending on clang version in use
        # this is safer commented for the time being
        # self.expect("expression si[0]", matching=False, error=True,
        #            substrs = ['first = ', 'zero'])

        lldbutil.continue_to_breakpoint(self.process(), bkpt)

        self.expect("frame variable si", substrs=["%s::map" % ns, "size=0", "{}"])

        lldbutil.continue_to_breakpoint(self.process(), bkpt)

        self.expect("frame variable is", substrs=["%s::map" % ns, "size=0", "{}"])

        lldbutil.continue_to_breakpoint(self.process(), bkpt)

        self.expect(
            "frame variable is",
            substrs=[
                "%s::map" % ns,
                "size=4",
                '[0] = (first = 1, second = "is")',
                '[1] = (first = 2, second = "smart")',
                '[2] = (first = 3, second = "!!!")',
                '[3] = (first = 85, second = "goofy")',
            ],
        )

        self.expect(
            "expression is",
            substrs=[
                "%s::map" % ns,
                "size=4",
                '[0] = (first = 1, second = "is")',
                '[1] = (first = 2, second = "smart")',
                '[2] = (first = 3, second = "!!!")',
                '[3] = (first = 85, second = "goofy")',
            ],
        )

        # check that MightHaveChildren() gets it right
        self.assertTrue(
            self.frame().FindVariable("is").MightHaveChildren(),
            "is.MightHaveChildren() says False for non empty!",
        )

        # check access-by-index
        self.expect("frame variable is[0]", substrs=["first = ", "second ="])

        # check that the expression parser does not make use of
        # synthetic children instead of running code
        # TOT clang has a fix for this, which makes the expression command here succeed
        # since this would make the test fail or succeed depending on clang version in use
        # this is safer commented for the time being
        # self.expect("expression is[0]", matching=False, error=True,
        #            substrs = ['first = ', 'goofy'])

        lldbutil.continue_to_breakpoint(self.process(), bkpt)

        self.expect("frame variable is", substrs=["%s::map" % ns, "size=0", "{}"])

        lldbutil.continue_to_breakpoint(self.process(), bkpt)

        self.expect("frame variable ss", substrs=["%s::map" % ns, "size=0", "{}"])

        lldbutil.continue_to_breakpoint(self.process(), bkpt)

        self.expect(
            "frame variable ss",
            substrs=[
                "%s::map" % ns,
                "size=3",
                '[0] = (first = "casa", second = "house")',
                '[1] = (first = "ciao", second = "hello")',
                '[2] = (first = "gatto", second = "cat")',
            ],
        )

        self.expect(
            "expression ss",
            substrs=[
                "%s::map" % ns,
                "size=3",
                '[0] = (first = "casa", second = "house")',
                '[1] = (first = "ciao", second = "hello")',
                '[2] = (first = "gatto", second = "cat")',
            ],
        )

        # check that MightHaveChildren() gets it right
        self.assertTrue(
            self.frame().FindVariable("ss").MightHaveChildren(),
            "ss.MightHaveChildren() says False for non empty!",
        )

        # check access-by-index
        self.expect("frame variable ss[2]", substrs=["gatto", "cat"])

        # check that the expression parser does not make use of
        # synthetic children instead of running code
        # TOT clang has a fix for this, which makes the expression command here succeed
        # since this would make the test fail or succeed depending on clang version in use
        # this is safer commented for the time being
        # self.expect("expression ss[3]", matching=False, error=True,
        #            substrs = ['gatto'])

        lldbutil.continue_to_breakpoint(self.process(), bkpt)

        self.expect("frame variable ss", substrs=["%s::map" % ns, "size=0", "{}"])

    @add_test_categories(["libc++"])
    def test_libcxx(self):
        self.build(dictionary={"USE_LIBCPP": 1})
        self.do_test()

    @add_test_categories(["libstdcxx"])
    def test_libstdcxx(self):
        self.build(dictionary={"USE_LIBSTDCPP": 1})
        self.do_test()

    @expectedFailureAll(
        bugnumber="Don't support formatting __gnu_debug::_Safe_iterator yet"
    )
    @add_test_categories(["libstdcxx"])
    def test_libstdcxx_debug(self):
        self.build(
            dictionary={"USE_LIBSTDCPP": 1, "CXXFLAGS_EXTRAS": "-D_GLIBCXX_DEBUG"}
        )
        self.do_test()

    @add_test_categories(["msvcstl"])
    def test_msvcstl(self):
        # No flags, because the "msvcstl" category checks that the MSVC STL is used by default.
        self.build()
        self.do_test(supports_end_iter=True)
