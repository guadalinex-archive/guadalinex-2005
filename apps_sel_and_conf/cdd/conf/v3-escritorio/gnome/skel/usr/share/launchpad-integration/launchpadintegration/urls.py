import urlparse
import subprocess
import os

def showUrl(url, logger=None):
    """Show the given URL in the user's preferred browser.

    Currently just shells out to gnome-open, which uses the browser
    picked in the Gnome "preferred applications" control panel.
    If Gnome is not available it uses the x-www-browser setting
    """
    if logger:
        logger.info('opening URL %s', url)
    
    if os.path.exists('/usr/bin/gnome-open'):
        command = ['gnome-open', url]
    else:
        command = ['x-www-browser', url]

    # check if we run from sudo (happens for e.g. gnome-system-tools, synaptic)
    if os.getuid() == 0 and os.environ.has_key('SUDO_USER'):
        command = ['sudo', '-u', os.environ['SUDO_USER']] + command

    p = subprocess.Popen(command,
                         close_fds=True,
                         stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE)
    p.communicate()
    return p.returncode

def launchpadDistroPrefix():
    distro = subprocess.Popen(['lsb_release', '--id', '--short'],
                              stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE).communicate()[0].strip()
    release = subprocess.Popen(['lsb_release', '--codename', '--short'],
                               stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE).communicate()[0].strip()
    return 'https://launchpad.net/distros/%s/%s/' % (distro.lower(), release)

def getSourcePackageUrl(pkginfo):
    prefix = launchpadDistroPrefix()
    return urlparse.urljoin(prefix, '+sources/%s/' % pkginfo.sourcepackage)

def getInfoUrl(pkginfo):
    #return urlparse.urljoin(getSourcePackageUrl(pkginfo), '+gethelp')
    return 'http://www.guadalinex.org'

def getTranslateUrl(pkginfo):
    #return urlparse.urljoin(getSourcePackageUrl(pkginfo), '+translate')
    return 'http://www.guadalinex.org'

# def getBugURL(pkginfo):
#     return urlparse.urljoin(getSourcePackageUrl(pkginfo), '+bugs')
