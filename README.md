RAS Daemon
==========

Those tools provide a way to get Platform Reliability, Availability
and Serviceability (RAS) reports made via the Kernel tracing events.

The main repository for the rasdaemon is at:

- <https://github.com/mchehab/rasdaemon>

And two mirrors are available:

- <https://gitlab.com/mchehab_kernel/rasdaemon>
- <http://git.infradead.org/users/mchehab/rasdaemon.git>

Tarballs for each release can be found at:
- <http://www.infradead.org/~mchehab/rasdaemon/>

GOALS
=====

Its initial goal is to replace the edac-tools that got bitroted after
the addition of the HERM (Hardware Events Report Method )patches[^1] at
the EDAC Kernel drivers.

[^1]: <http://lkml.indiana.edu/hypermail/linux/kernel/1205.1/02075.html>

Its long term goal is to be the userspace tool that will collect all
hardware error events reported by the Linux Kernel from several sources
(EDAC, MCE, PCI, ...) into one common framework.

It is not meant to provide tools for doing error injection, as there are
other tools already covering it, like:
<git://git.kernel.org/pub/scm/linux/kernel/git/gong.chen/mce-test.git>

Yet, a few set of testing scripts are provided under /contrib dir.

When the final version of the HERM patches was merged upstream, it was
decided to not expose the memory error counters to userspace.
This is one of the differences from what it was provided by edac-utils, as
EDAC 2.0.0 exports errors via a set of sysfs nodes that sums the amount of
errors per DIMM, per memory channel and per memory controller.
However, those counters are monotonically increased, and there's no way to
detect if they're very sparsed in time, if the occurrence is increasing over
time, or if they're due to some burst, perhaps due to a Solar Storm hitting
the ionosphere.

In other words, the rationale for not exposing such the information is that:


1. can be easily accounted on userspace;
2. they're not really meaningful. E. g. one system with, let's say
   10 corrected errors can be fine, while another one with the same amount
   of errors can have problems, as the error counters don't take into
   account things like system uptime, memory error bursts (that could be
   caused by a solar storm, for example), etc.

So, the idea since them was to make the kernel-userspace interface
simpler and move the policy to the userspace daemon. It is up to the
userspace daemon to correlate the data about the RAS events and provide
the system administrator a comprehensive report, presenting him
a better hint if he needs to contact the hardware vendor to replace
a component that is working degraded, or to simply discard the error.

So, the approach taken here is to allow storing those errors on a SQLite
database, in order to allow those data to be latter mining.

It is currently not part of the scope to do sophiscicated data mininy
analysis, as that would require enough statistitical data about hardware
MTBF. In other words, an abnormal component that needs to be replaced
shoud be statistically compared with a similar component that operates
under a normal condition. To do such checks, the analysis tool would
need to know the probability density function(p. d. f.) of that component,
and its rellevant parameters (like mean and standard derivation, if the
p. d. f. funcion is a Normal distribution).

While this tool works since Kernel 3.5 (where HERM patches got added),
in order to get the full benefit of this tool, Kernel 3.10 or upper is
needed.

COMPILING AND INSTALLING
========================

sqlite3 and autoconf needs to be installed. On Fedora, this is done by
installing the following packages:

```
    make
    gcc
    autoconf
    automake
    libtool
    libtraceevent-devel
    tar
    sqlite-devel	(if sqlite3 will be used)
    perl-DBD-SQLite	(if sqlite3 will be used)
```

To install then on Fedora, run:
```
    $ dnf install -y make gcc autoconf automake libtool tar perl-dbd-sqlite \
      libtraceevent-devel
```
Or, if sqlite3 database will be used to store data:

```
    $ dnf install -y make gcc autoconf automake libtool tar sqlite-devel \
      libtraceevent-devel
```

There are currently 3 features that are enabled optionally, via
./configure parameters:

```
    --enable-sqlite3      enable storing data at SQL lite database (currently
			  experimental)
    --enable-aer            enable PCIe AER events (currently experimental)
    --enable-mce            enable MCE events (currently experimental)
```

In order to compile it, run:
```
    $ autoreconf -vfi
    $ ./configure [parameters]
    $ make
```

So, for example, to enable everything but sqlite3:

```
    $ autoreconf -vfi && ./configure --enable-aer --enable-mce && make
```

After compiling, run, as root:
```
    $ make install
```

RPM-based compilation
=====================

If the distribution is rpm-based, an alternative method would be to do:
```
    $ autoreconf -vfi && ./configure
```

The above procedure will generate a file at misc/rasdaemon.spec.

You may edit it, in order to add/remove the --enable-\[option\]
parameters.

To generate the rpm files, do:

```
    # make mock
```

To install the rpm files, run, as root:
```
    # rpm -i $(ls SRPMS/rasdaemon-*.rpm|tail -1)
```

RUNNING
=======

The daemon generally requires root permission, in order to read the
needed debugfs trace nodes, with needs to be previously mounted.
The rasdaemon will check at /proc/mounts where the debugfs partition
is mounted and use it while running.

To run the rasdaemon in background, just call it without any parameters:

```
    # rasdaemon
```

The output will be available via syslog. Or, to run it in foreground and see
the logs in console, run it as:

```
    # rasdaemon -f
```

or, if you also want to record errors at the database (--enable-sqlite3 is
required):

```
    # rasdaemon -f -r
```

To post-process and decode received MCA errors on AMD SMCA systems, run:

```
	# rasdaemon -p --status <STATUS_reg> --ipid <IPID_reg> --smca --family <CPU Family> --model <CPU Model> --bank <BANK_NUM>
```

Status and IPID Register values (in hex) are mandatory. The `smca` flag
with `family` and `model` are required if not decoding locally. `Bank`
parameter is optional.

You may also start it via systemd:

```
    # systemctl start rasdaemon
```

The rasdaemon will then output the messages to journald.

TESTING
=======

A script is provided under /contrib, in order to test the daemon EDAC
handler. While the daemon is running, just run:

```
# contrib/edac-fake-inject
```

The script requires a Kernel compiled with CONFIG_EDAC_DEBUG and a running
EDAC driver.

MCE error handling can use the MCE inject:
<https://git.kernel.org/pub/scm/utils/cpu/mce/mce-inject.git>

For it to work, Kernel mce-inject module should be compiled and loaded.

APEI error injection can use this tool:
<https://git.kernel.org/pub/scm/linux/kernel/git/gong.chen/mce-test.git/>

AER error injection can use this tool:
<https://git.kernel.org/pub/scm/linux/kernel/git/gong.chen/aer-inject.git/>

# SUBMITTING PATCHES

If you want to help improving this tool, be my guest! We try to follow
the Kernel's CodingStyle and submission rules as a reference.

Before submitting your patch, please check the coding style with:
scripts/checkpatch.pl.

In order to contribute with rasdaemon, please send a Merge Request via
github repository at:

- <https://github.com/mchehab/rasdaemon>

Or, alternatively, send a pull request against gitlab repository at:

- <https://gitlab.com/mchehab_kernel/rasdaemon>

It is also recommended to send patches to <linux-edac@vger.kernel.org>
with a copy to:

- Mauro Carvalho Chehab \<<mchehab@kernel.org>\>

Please notice that github is the preferred way. If you're not using
it, please be kind enough to add an issue there for us to track the
patch series.

Don't foget to add a description of the patch in the body of the email, adding
a Signed-off-by: at the end of the patch description (before the unified diff
with the patch).

We use Signed-off-by the same way as in kernel, so I'm transcribing
bellow the same text as found under Kernel's Documentation/SubmittingPatches:

```
   "To improve tracking of who did what, especially with patches that can
    percolate to their final resting place in the kernel through several
    layers of maintainers, we've introduced a "sign-off" procedure on
    patches that are being emailed around.

    The sign-off is a simple line at the end of the explanation for the
    patch, which certifies that you wrote it or otherwise have the right to
    pass it on as an open-source patch.  The rules are pretty simple: if you
    can certify the below:

	    Developer's Certificate of Origin 1.1

	    By making a contribution to this project, I certify that:

	    (a) The contribution was created in whole or in part by me and I
		have the right to submit it under the open source license
		indicated in the file; or

	    (b) The contribution is based upon previous work that, to the best
		of my knowledge, is covered under an appropriate open source
		license and I have the right under that license to submit that
		work with modifications, whether created in whole or in part
		by me, under the same open source license (unless I am
		permitted to submit under a different license), as indicated
		in the file; or

	    (c) The contribution was provided directly to me by some other
		person who certified (a), (b) or (c) and I have not modified
		it.

	    (d) I understand and agree that this project and the contribution
		are public and that a record of the contribution (including all
		personal information I submit with it, including my sign-off) is
		maintained indefinitely and may be redistributed consistent with
		this project or the open source license(s) involved.

    then you just add a line saying

	    Signed-off-by: Random J Developer <random@developer.example.org>

    using your real name (sorry, no pseudonyms or anonymous contributions.)"

```
