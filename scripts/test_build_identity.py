"""Check that volatile build identity reaches only BuildInfo.cpp."""
from pathlib import Path
from unittest import mock
import os
import runpy
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = runpy.run_path(str(ROOT / "scripts/git_branch.py"))

class Env(dict):
    def __init__(self, name="default"):
        super().__init__(PROJECT_DIR=str(ROOT), PIOENV=name)
        self.defines = []
        self.middleware = []
    def Append(self, **kwargs): self.defines.extend(kwargs.get("CPPDEFINES", []))
    def Clone(self):
        copy = Env(self["PIOENV"])
        copy.defines = list(self.defines)
        return copy
    def Object(self, node): return (node, self.defines)
    def AddBuildMiddleware(self, callback, pattern): self.middleware.append((callback, pattern))

class BuildIdentityTest(unittest.TestCase):
    def test_identity_only_changes_one_compile_environment(self):
        function = MODULE["inject_version"]
        globals_ = function.__globals__
        previous = globals_["get_git_short_sha"]
        self.addCleanup(globals_.__setitem__, "get_git_short_sha", previous)
        for name in ("default", "sticky", "x4-pro", "x4-classic", "debug", "sticky-debug", "x4-pro-debug", "test", "gh_release_rc", "x4-pro-simulator"):
            captures = []
            for sha in ("abc1234", "def5678"):
                globals_["get_git_short_sha"] = lambda _: sha
                env = Env(name)
                function(env)
                self.assertEqual(len(env.middleware), 1)
                callback, pattern = env.middleware[0]
                self.assertEqual(pattern, "*src/util/BuildInfo.cpp")
                _, scoped = callback(env, "BuildInfo.cpp")
                names = [d[0] for d in env.defines if isinstance(d, tuple)]
                self.assertNotIn("CROSSINK_GIT_SHA", names)
                self.assertNotIn("CROSSINK_GIT_DIRTY", names)
                self.assertNotIn("CROSSINK_VERSION", names)
                self.assertIn("CROSSINK_GIT_SHA", dict(d for d in scoped if isinstance(d, tuple)))
                captures.append((env.defines, scoped))
            self.assertEqual(captures[0][0], captures[1][0])
            self.assertNotEqual(captures[0][1], captures[1][1])

class VersionStringTest(unittest.TestCase):
    """Pin the CROSSINK_VERSION formats the OTA version check relies on."""

    def version(self, pioenv, **environ):
        function = MODULE["inject_version"]
        fakes = {
            "get_crossink_version": lambda _: "0.1.0",
            "get_git_branch": lambda _: "fix-rcfoo",
            "get_git_short_hash": lambda _, length=5: "a1b2c",
            "get_git_short_sha": lambda _: "a1b2c3d",
        }
        clean = {k: v for k, v in os.environ.items() if not k.startswith("CROSSINK_")}
        with mock.patch.dict(function.__globals__, fakes), mock.patch.dict(os.environ, {**clean, **environ}, clear=True):
            env = Env(pioenv)
            function(env)
            callback, _ = env.middleware[0]
            _, scoped = callback(env, "BuildInfo.cpp")
        return dict(d for d in scoped if isinstance(d, tuple))["CROSSINK_VERSION"].strip('\\"')

    def test_local_builds_are_marked_dev(self):
        self.assertEqual(self.version("default"), "0.1.0-dev+fix-rcfoo")
        self.assertEqual(self.version("sticky"), "0.1.0-dev+fix-rcfoo-sticky")
        self.assertEqual(self.version("x4-pro"), "0.1.0-dev+fix-rcfoo-x4-pro")
        self.assertEqual(self.version("x4-classic"), "0.1.0-dev+fix-rcfoo-x4-classic")

    def test_stable_versions_are_unchanged(self):
        self.assertEqual(self.version("default", CROSSINK_RELEASE_VERSION="v0.1.0"), "0.1.0")
        self.assertEqual(self.version("sticky", CROSSINK_RELEASE_VERSION="0.1.0"), "0.1.0-sticky")
        self.assertEqual(self.version("x4-pro", CROSSINK_RELEASE_VERSION="0.1.0"), "0.1.0-x4-pro")
        self.assertEqual(self.version("x4-classic", CROSSINK_RELEASE_VERSION="0.1.0"), "0.1.0-x4-classic")

    def test_nightly_versions_are_unchanged(self):
        nightly = "0.1.0-nightly.20261201"
        self.assertEqual(self.version("default", CROSSINK_RELEASE_VERSION=nightly), nightly)
        self.assertEqual(self.version("x4-pro", CROSSINK_RELEASE_VERSION=nightly), nightly + "-x4-pro")
        self.assertEqual(self.version("x4-classic", CROSSINK_RELEASE_VERSION=nightly), nightly + "-x4-classic")

    def test_rc_and_debug_versions_are_unchanged(self):
        self.assertEqual(self.version("default", CROSSINK_RC_HASH="a1b2c"), "0.1.0-a1b2c-RC")
        self.assertEqual(self.version("sticky", CROSSINK_RC_HASH="a1b2c"), "0.1.0-a1b2c-RC")
        self.assertEqual(self.version("gh_release_rc", CROSSINK_RC_HASH="a1b2c"), "0.1.0-a1b2c-RC")
        self.assertEqual(self.version("debug"), "0.1.0-fix-rcfoo+a1b2c")
        self.assertEqual(self.version("x4-pro-debug"), "0.1.0-fix-rcfoo+a1b2c")

if __name__ == "__main__": unittest.main()
