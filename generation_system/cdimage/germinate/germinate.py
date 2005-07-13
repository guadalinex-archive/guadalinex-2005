#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# arch-tag: e7f7b26b-95bc-4432-86e6-832a7cc5ac01
"""Update list files from the Wiki.
"""

# Copyright (c) 2004, 2005 Canonical Ltd.
#
# Germinate is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any
# later version.
#
# Germinate is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Germinate; see the file COPYING.  If not, write to the Free
# Software Foundation, 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

import apt_pkg
import gzip
import os
import shutil
import sys
import urllib
import getopt
import logging
import codecs
from Germinate import Germinator
import Germinate.Archive


# Where do we get up-to-date seeds from?
SEEDS = "http://people.ubuntu.com/~cjwatson/seeds/"
RELEASE = "breezy"

# If we need to download Packages.gz and/or Sources.gz, where do we get
# them from?
MIRROR = "http://archive.ubuntu.com/ubuntu/"
DIST = ["breezy"]
COMPONENTS = ["main"]
ARCH = "i386"

CHECK_IPV6 = False

# If we need to download a new IPv6 dump, where do we get it from?
IPV6DB = "http://debdev.fabbione.net/stat/"


def open_ipv6_tag_file(filename):
    """Download the daily IPv6 db dump if needed, and open it."""
    if os.path.exists(filename):
        return open(filename, "r")

    print "Downloading", filename, "file ..."
    url = IPV6DB + filename + ".gz"
    gzip_fn = None
    try:
        gzip_fn = urllib.urlretrieve(url, filename + ".gz")[0]
        print "Decompressing", filename, "file ..."
        gzip_f = gzip.GzipFile(filename=gzip_fn)
        f = open(filename, "w")
        for line in gzip_f:
            print >>f, line,
        f.close()
        gzip_f.close()
    finally:
        if gzip_fn is not None:
            os.unlink(gzip_fn)

    return open(filename, "r")

def open_metafile(filename):
    try:
	print "SEEDS", SEEDS, "."
	print "RELEASE", RELEASE, "."
        url = SEEDS + RELEASE + "/" + filename
        print "Downloading", url, "..."
        urlopener = urllib.FancyURLopener()
        urlopener.addheader('Cache-Control', 'no-cache')
        urlopener.addheader('Pragma', 'no-cache')
        return urlopener.open(url)
    except IOError:
        return None

def write_list(whyname, filename, g, pkglist):
    pkg_len = len("Package")
    src_len = len("Source")
    why_len = len("Why")
    mnt_len = len("Maintainer")

    for pkg in pkglist:
        _pkg_len = len(pkg)
        if _pkg_len > pkg_len: pkg_len = _pkg_len

        _src_len = len(g.packages[pkg]["Source"])
        if _src_len > src_len: src_len = _src_len

        _why_len = len(g.why[whyname][pkg][0])
        if _why_len > why_len: why_len = _why_len

        _mnt_len = len(g.packages[pkg]["Maintainer"])
        if _mnt_len > mnt_len: mnt_len = _mnt_len

    size = 0
    installed_size = 0

    pkglist.sort()
    f = codecs.open(filename, "w", "utf8", "replace")
    print >>f, "%-*s | %-*s | %-*s | %-*s | %-15s | %-15s" % \
          (pkg_len, "Package",
           src_len, "Source",
           why_len, "Why",
           mnt_len, "Maintainer",
           "Deb Size (B)",
           "Inst Size (KB)")
    print >>f, ("-" * pkg_len) + "-+-" + ("-" * src_len) + "-+-" \
          + ("-" * why_len) + "-+-" + ("-" * mnt_len) + "-+-" \
          + ("-" * 15) + "-+-" + ("-" * 15) + "-"
    for pkg in pkglist:
        size += g.packages[pkg]["Size"]
        installed_size += g.packages[pkg]["Installed-Size"]
        print >>f, "%-*s | %-*s | %-*s | %-*s | %15d | %15d" % \
              (pkg_len, pkg,
               src_len, g.packages[pkg]["Source"],
               why_len, g.why[whyname][pkg][0],
               mnt_len, g.packages[pkg]["Maintainer"],
               g.packages[pkg]["Size"],
               g.packages[pkg]["Installed-Size"])
    print >>f, ("-" * (pkg_len + src_len + why_len + mnt_len + 9)) + "-+-" \
          + ("-" * 15) + "-+-" + ("-" * 15) + "-"
    print >>f, "%*s | %15d | %15d" % \
          ((pkg_len + src_len + why_len + mnt_len + 9), "",
           size, installed_size)

    f.close()

def write_source_list(filename, g, srclist):
    global CHECK_IPV6

    src_len = len("Source")
    mnt_len = len("Maintainer")
    ipv6_len = len("IPv6 status")

    for src in srclist:
        _src_len = len(src)
        if _src_len > src_len: src_len = _src_len

        _mnt_len = len(g.sources[src]["Maintainer"])
        if _mnt_len > mnt_len: mnt_len = _mnt_len

        if CHECK_IPV6:
            _ipv6_len = len(g.sources[src]["IPv6"])
            if _ipv6_len > ipv6_len: ipv6_len = _ipv6_len

    srclist.sort()
    f = codecs.open(filename, "w", "utf8", "replace")

    format = "%-*s | %-*s"
    header_args = [src_len, "Source", mnt_len, "Maintainer"]
    separator = ("-" * src_len) + "-+-" + ("-" * mnt_len) + "-"
    if CHECK_IPV6:
        format += " | %-*s"
        header_args.extend((ipv6_len, "IPv6 status"))
        separator += "+-" + ("-" * ipv6_len) + "-"

    print >>f, format % tuple(header_args)
    print >>f, separator
    for src in srclist:
        args = [src_len, src, mnt_len, g.sources[src]["Maintainer"]]
        if CHECK_IPV6:
            args.extend((ipv6_len, g.sources[src]["IPv6"]))
        print >>f, format % tuple(args)

    f.close()

def write_rdepend_list(filename, g, pkg):
    f = open(filename, "w")
    print >>f, pkg
    _write_rdepend_list(f, g, pkg, "", done=[])
    f.close()

def _write_rdepend_list(f, g, pkg, prefix, stack=None, done=None):
    if stack is None:
        stack = []
    else:
        stack = list(stack)
        if pkg in stack:
            print >>f, prefix + "! loop"
            return
    stack.append(pkg)

    if done is None:
        done = []
    elif pkg in done:
        print >>f, prefix + "! skipped"
        return
    done.append(pkg)

    for seed in g.seeds:
        if pkg in g.seed[seed]:
            print >>f, prefix + "*", seed.title(), "seed"

    if "Reverse-Depends" not in g.packages[pkg]:
        return

    for field in ("Pre-Depends", "Depends",
                  "Build-Depends", "Build-Depends-Indep"):
        if field not in g.packages[pkg]["Reverse-Depends"]:
            continue

        i = 0
        print >>f, prefix + "*", "Reverse", field + ":"
        for dep in g.packages[pkg]["Reverse-Depends"][field]:
            i += 1
            print >>f, prefix + " +- " + dep
            if field.startswith("Build-"):
                continue

            if i == len(g.packages[pkg]["Reverse-Depends"][field]):
                extra = "    "
            else:
                extra = " |  "
            _write_rdepend_list(f, g, dep, prefix + extra, stack, done)

def write_prov_list(filename, provdict):
    provides = provdict.keys()
    provides.sort()

    f = open(filename, "w")
    for prov in provides:
        print >>f, prov

        provlist = provdict[prov]
        provlist.sort()
        for pkg in provlist:
            print >>f, "\t%s" % (pkg,)
        print >>f
    f.close()


def usage(f):
    print >>f, """Usage: germinate.py [options]

Options:

  -h, --help            Print this help message.
  -S, --seed-source=SOURCE
                        Fetch seeds from SOURCE
                        (default: %s).
  -s, --seed-dist=DIST  Fetch seeds for distribution DIST (default: %s).
  -m, --mirror=MIRROR   Get package lists from MIRROR
                        (default: %s).
  -d, --dist=DIST       Operate on distribution DIST (default: %s).
  -a, --arch=ARCH       Operate on architecture ARCH (default: %s).
  -c, --components=COMPS
                        Operate on components COMPS (default: %s).
  -i, --ipv6            Check IPv6 status of source packages.
  --no-rdepends         Disable reverse-dependency calculations.
""" % (SEEDS, RELEASE, MIRROR, ",".join(DIST), ARCH, ",".join(COMPONENTS))


def main():
    global SEEDS, RELEASE, MIRROR, DIST, ARCH, COMPONENTS, CHECK_IPV6
    want_rdepends = True
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)
    handler = logging.StreamHandler(sys.stdout)
    handler.setFormatter(logging.Formatter('%(levelname)s%(message)s'))
    logger.addHandler(handler)

    g = Germinator()

    try:
        opts, args = getopt.getopt(sys.argv[1:], "hS:s:m:d:c:a:i",
                                   ["help",
                                    "seed-source=",
                                    "seed-dist=",
                                    "mirror=",
                                    "dist=",
                                    "components=",
                                    "arch=",
                                    "ipv6",
                                    "no-rdepends"])
    except getopt.GetoptError:
        usage(sys.stderr)
        sys.exit(2)

    for option, value in opts:
        if option in ("-h", "--help"):
            usage(sys.stdout)
            sys.exit()
        elif option in ("-S", "--seed-source"):
            SEEDS = value
            if not SEEDS.endswith("/"):
                SEEDS += "/"
        elif option in ("-s", "--seed-dist"):
            RELEASE = value
        elif option in ("-m", "--mirror"):
            MIRROR = value
            if not MIRROR.endswith("/"):
                MIRROR += "/"
        elif option in ("-d", "--dist"):
            DIST = value.split(",")
        elif option in ("-c", "--components"):
            COMPONENTS = value.split(",")
        elif option in ("-a", "--arch"):
            ARCH = value
        elif option in ("-i", "--ipv6"):
            CHECK_IPV6 = True
        elif option == "--no-rdepends":
            want_rdepends = False

    apt_pkg.InitConfig()
    apt_pkg.Config.Set("APT::Architecture", ARCH)

    Germinate.Archive.TagFile(MIRROR).feed(g, DIST, COMPONENTS, ARCH)

    if CHECK_IPV6:
        g.parseIPv6(open_ipv6_tag_file("dailydump"))

    if os.path.isfile("hints"):
        g.parseHints(open("hints"))

    blacklist = open_metafile("blacklist")
    if blacklist is not None:
        g.parseBlacklist(blacklist)

    (seednames, seedinherit) = g.parseStructure(open_metafile("STRUCTURE"))
    for seedname in seednames:
        g.plantSeed(SEEDS, RELEASE, ARCH, seedname,
                    list(seedinherit[seedname]))
    g.prune()
    g.grow()
    g.addExtras()
    if want_rdepends:
        g.reverseDepends()

    seednames_extra = list(seednames)
    seednames_extra.append('extra')
    for seedname in seednames_extra:
        write_list(seedname, seedname,
                   g, g.seed[seedname] + g.depends[seedname])
        write_list(seedname, seedname + ".seed",
                   g, g.seed[seedname])
        write_list(seedname, seedname + ".depends",
                   g, g.depends[seedname])
        write_list(seedname, seedname + ".build-depends",
                   g, g.build_depends[seedname])

        if seedname != "extra":
            write_source_list(seedname + ".sources",
                              g, g.sourcepkgs[seedname])
        write_source_list(seedname + ".build-sources",
                          g, g.build_sourcepkgs[seedname])

    all = []
    sup = []
    all_srcs = []
    sup_srcs = []
    for seedname in seednames:
        all += g.seed[seedname]
        all += g.depends[seedname]
        all += g.build_depends[seedname]
        all_srcs += g.sourcepkgs[seedname]
        all_srcs += g.build_sourcepkgs[seedname]

        if seedname == "supported":
            sup += g.seed[seedname]
            sup += g.depends[seedname]
            sup_srcs += g.sourcepkgs[seedname]

        # Only include those build-dependencies that aren't already in the
        # dependency outputs for inner seeds of supported. This allows
        # supported+build-depends to be usable as an "everything else"
        # output.
        build_depends = dict.fromkeys(g.build_depends[seedname], True)
        build_sourcepkgs = dict.fromkeys(g.build_sourcepkgs[seedname], True)
        for seed in g.innerSeeds("supported"):
            build_depends.update(dict.fromkeys(g.seed[seed], False))
            build_depends.update(dict.fromkeys(g.depends[seed], False))
            build_sourcepkgs.update(dict.fromkeys(g.sourcepkgs[seed], False))
        sup += [k for (k, v) in build_depends.iteritems() if v]
        sup_srcs += [k for (k, v) in build_sourcepkgs.iteritems() if v]

    # TODO: use sets from Python 2.4 once Ubuntu datacentre is upgraded to
    # Hoary
    all = dict.fromkeys(all).keys()
    all_srcs = dict.fromkeys(all_srcs).keys()
    sup = dict.fromkeys(sup).keys()
    sup_srcs = dict.fromkeys(sup_srcs).keys()

    write_list("all", "all", g, all)
    write_source_list("all.sources", g, all_srcs)

    write_list("all", "supported+build-depends", g, sup)
    write_source_list("supported+build-depends.sources", g, sup_srcs)

    write_list("all", "all+extra", g, g.all)
    write_source_list("all+extra.sources", g, g.all_srcs)

    write_prov_list("provides", g.pkgprovides)

    if os.path.exists("rdepends"):
        shutil.rmtree("rdepends")
    if want_rdepends:
        os.mkdir("rdepends")
        os.mkdir(os.path.join("rdepends", "ALL"))
        for pkg in g.all:
            dirname = os.path.join("rdepends", g.packages[pkg]["Source"])
            if not os.path.exists(dirname):
                os.mkdir(dirname)

            write_rdepend_list(os.path.join(dirname, pkg), g, pkg)
            os.symlink(os.path.join("..", g.packages[pkg]["Source"], pkg),
                       os.path.join("rdepends", "ALL", pkg))

    g.writeBlacklisted("blacklisted")

if __name__ == "__main__":
    main()
