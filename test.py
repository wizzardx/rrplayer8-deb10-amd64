#!/usr/bin/python3 -u

#"""Do a relatively complete test of our project.

#Sets up a VM, runs automated tests, installs the deb package, shows us log
#files, gives the VM host a copy of the final built deb package, etc.

#Docstring formatting follows this spec:
    #http://sphinxcontrib-napoleon.readthedocs.org/en/latest/example_google.html

#Type Hints follow PEP 484:
    #https://www.python.org/dev/peps/pep-0484/

#"""

## pylint:disable=fixme


# For now we just hardcode the Ruby version to install
RUBY_VERSION = '2.7.1'


from os import chdir, getcwd, remove, makedirs, rename, symlink, mkdir
import os
import sys

# pylint: disable=ungrouped-imports
from os.path import isfile, join, isdir, basename, dirname
# pylint: enable=ungrouped-imports

from pprint import pprint as pp
from shutil import copy2, rmtree
from subprocess import check_call, check_output, call
from time import sleep


# Make Mypy static checks happy:

# pylint: disable=unused-import
if False:  # pylint: disable=using-constant-test
    from typing import Any  # noqa
# pylint: enable=unused-import


# Attempt to load yaml support
try:
    import yaml
except ImportError:
    raise Exception('Please run "sudo apt-get install python3-yaml"')


def get_cmd_from_args() -> str:
    """Get something like '' or 'run_under_vm' from the command-line.

    This helps our script decide which logic to invoke.

    """
    if len(sys.argv) == 1:
        return ""
    else:
        return sys.argv[1]


def load_project_settings() -> 'Dict[str, Any]':
    """Load the app project settings.

    Loads and decodes the "project_settings.yaml".

    This gives us useful details like deb package name, version, dependencies,
    virtual machine details, etc.

    Returns:
        Dict[str, Any]: Settings loaded, in a python dict object.

    """
    with open("project_settings.yaml") as yaml_file:
        return yaml.safe_load(yaml_file)


def run_test_under_vm(cfg: 'Dict[str,Any]') -> None:
    """Run our testing logic under the VM.

    Also do some minimal required config so that our testing logic
    (test.py) will be able to run.

    Args:
        cfg (Dict[str,Any]): Configuration loaded from project_settings.yaml

    """
    # Create a temporary shell script which we run under the VM.
    vm_cfg = cfg['test_vm_settings']
    tmp_setup_script = (
        '.tmp_b22e71aa-db7c-11e5-821b-c3c48b16dff1_run'
        '_tests_under_vm.sh')
    try:
        shell_logic = """#!/bin/bash
set -e

#Export some additional environment variables to help the test script
#that will run under the VM.

if [ ! -f /usr/lib/python3/dist-packages/yaml/__init__.py ]; then
    echo "deb %s %s main contrib non-free" > /etc/apt/sources.list
    apt-get update
    apt-get install -y --force-yes python3-yaml
fi

#Next, change over to our project directory and run the under-VM testing
#logic:
cd /vagrant
./test.py run_under_vm

""" % (vm_cfg['deb_mirror_urls']['debian'],
       vm_cfg['debian_release_name'])

        with open(tmp_setup_script, 'w') as script_file:
            script_file.write(shell_logic)
        check_call(['chmod', '+x', tmp_setup_script])

        os.environ['SSHPASS'] = 'vagrant'

        ssh_cmd = "sshpass -e ssh -p 2222 vagrant@localhost -C "
        check_call(ssh_cmd + 'sudo /vagrant/%s' % tmp_setup_script, shell=True)

    finally:
        # At the end we tidy up our temporary shell script:
        if isfile(tmp_setup_script):
            remove(tmp_setup_script)


def run_dev_box_logic() -> None:
    """Run needed logic on developer's PC.

    Does some initial logic (updated custom Mypy lib if available), then sets
    up a virtualbox VM, and then runs the rest of the test logic inside there.

    Raises:
        Exception: For general problems detected in the dev box setup.

    """
    # Load project settings from yaml config file:
    cfg = load_project_settings()

    # Check if the user has the "approx" proxy setup and configured on their
    # dev PC:
    if not isfile('/etc/approx/approx.conf'):
        raise Exception('Please install and configure approx. '
                        'See doc/approx_setup.txt for more info.')

    ## TODO: Run mypy to check our misc python source code logic:
    #assert 0

    # Check if the 'virtualbox' program is installed:
    if not isfile('/usr/bin/virtualbox'):
        raise Exception('Please install VirtualBox')

    # Check if the 'vagrant' utility is installed:
    if not isfile('/usr/bin/vagrant'):
        raise Exception('Please run "sudo apt-get install vagrant"')

    # Create a virtualbox VM if we don't already have one:
    if not isfile('Vagrantfile'):
        vbox_name = cfg['test_vm_settings']['vagrant_box_name']
        check_call(['vagrant', 'init', vbox_name])

    # Bring the VM online:
    print('REMINDER: Run "vagrant destroy" later to remove the temporary '
          'testing VM.')
    check_call(['vagrant', 'up'])

    # Run our testing logic under the VM. This also does any required
    # bootstrapping for the test script (test.py) to be able to run.
    run_test_under_vm(cfg)

    # Print the VM reminder again:
    print('REMINDER: Run "vagrant destroy" later to remove the temporary '
          'testing VM.')


class pushdir:
    def __init__(self, dirname: str) -> None:
        self._old_dir = getcwd()
        chdir(dirname)

    def __enter__(self) -> 'pushdir':
        return self

    def __exit__(self, *args: 'Any') -> None:
        chdir(self._old_dir)


def build_player_bin(cfg: 'Dict[str,Any]') -> None:

    with pushdir('src'):
        if not isfile('/tmp/.cpp_build_deps_installed'):
#            check_call(['apt-get', 'install', '-y', 'meson', 'g++', 'libglib2.0-dev', 'libpqxx-dev', 'libcurlpp-dev'])
            check_call(['apt-get', 'install', '-y', '--force-yes', 'meson', 'g++', 'libglib2.0-dev', 'libpqxx-dev', 'libxmlrpc-c++8-dev', 'libssl-dev'])
            with open('/tmp/.cpp_build_deps_installed', 'w') as f:
                pass

        # Generate config.h and common/config.h
        for name in ['config.h', 'common/config.h']:
            with open(name, 'w') as f:
                f.write('#ifndef CONFIG_H\n')
                f.write('#define CONFIG_H\n')
                f.write('#define PACKAGE "player"\n')
                f.write('#define VERSION "%s"\n' % cfg['package']['version'])
                f.write('#endif\n')

        if not isdir('build'):
            mkdir('build')
        chdir('build')
        if not isfile('build.ninja'):
            check_call(['meson', '..'])
        check_call(['ninja'])


# pylint: disable=too-many-locals,too-many-statements
def build_deb_installer(cfg: 'Dict[str,Any]') -> None:
    """Build a deb installer for our project.

    Args:
        cfg (Dict[str,Any]): configuration loaded from project_settings.yaml

    """
    # Make sure we are working under /tmp, not under /vagrant:
    chdir('/tmp/vagrant_vm_test')

    # Build the C++ source code into a player binary:
    build_player_bin(cfg)

    # Create a temporary directory under which we'll store the deb installer
    # files:
    pkg_name = cfg['package']['name']
    pkg_dir = "tmp/packages/%s" % pkg_name
    if isdir(pkg_dir):
        rmtree(pkg_dir)
    makedirs(pkg_dir)

    # Create a player directory to put our instore player under:
    player_dir = join(pkg_dir, "data/radio_retail/progs/player")
    makedirs(player_dir)

    # Put our instore player binary under there:
    copy2('src/build/player', player_dir + '/')

    # Also copy over the main player config file:
    copy2('cfg/player.conf', player_dir + '/')

    # Create a directory under which the older instore player log files can go:
    old_player_logs_dir = join(player_dir, 'logs')
    makedirs(old_player_logs_dir)

    # Create a /usr/share directory under which we put a bunch of additional
    # files:
    usr_share_dir = join(pkg_dir, "usr/share/rrplayer8")
    makedirs(usr_share_dir)

    # Copy our mpd config files:
    copy2('cfg/mpd_1.conf', usr_share_dir + '/')
    copy2('cfg/mpd_2.conf', usr_share_dir + '/')

    # Copy some Python dependencies over:
    copy2('deb_extras/python/python-mpd2-0.5.5.tar.gz', usr_share_dir + '/')

    # Copy over our Python scripts:
    copy2('src/python_logic/fake_xmms_api.py', usr_share_dir + '/')
    copy2('src/python_logic/traceback2.py', usr_share_dir + '/')

    # Copy over an init.d script:
    etc_initd_dir = join(pkg_dir, "etc/init.d")
    makedirs(etc_initd_dir)
    copy2('deb_extras/initd_script.sh', etc_initd_dir + '/rrplayer8')

    # Make misc directories for mpd:
    for num in {1, 2}:
        makedirs(join(pkg_dir, "var/log/rrplayer8/mpd/%d" % num))
        makedirs(join(pkg_dir, "var/lib/rrplayer8/mpd/%d/playlists" % num))
        makedirs(join(pkg_dir, "var/lib/rrplayer8/mpd/%d/music" % num))
        makedirs(join(pkg_dir, "var/run/rrplayer8/mpd/%d" % num))

    # Use FPM to build the deb package:
    pkg_ver = cfg['package']['version']
    deb_user = deb_group = cfg['package']['name']
    pkg_maintainer = cfg['package']['maintainer']
    pkg_description = cfg['package']['description']
    pkg_url = cfg['package']['url']

    fpm_cmd = [
        "rbenv", "exec", "fpm",
        "-s", "dir",
        "-t", "deb",
        "-a", "amd64",
        "-n", pkg_name,
        "-v", pkg_ver,
        "--force",
        "--deb-user", deb_user,
        "--deb-group", deb_group,
        "--license", "Proprietary",
        "--vendor", "Full Facing (Pty) Ltd",
        "--category", "service",
        "--maintainer", pkg_maintainer,
        "--description", pkg_description,
        "--url", pkg_url,
        #"--deb-compression", "xz",
        "--before-install", "debian/preinst",
        "--after-install", "debian/postinst",
        "--after-remove", "debian/postrm",
    ]

    fpm_pkg_deps = []  # type: List[str]
    for dep in cfg['package']['dependencies']:
        fpm_pkg_deps += ['-d', dep]

    fpm_cmd += fpm_pkg_deps

    fpm_cmd += [
        "-C", pkg_dir,
        "./"
    ]

    # Build the deb file:
    check_call(fpm_cmd)
# pylint: enable=too-many-locals,too-many-statements


def run_qa_tests_after_pkg_install(cfg: 'Dict[str,Any]') -> None:
    # Enable sound under virtualbox VM...
    #check_call('modprobe snd', shell=True)
    #check_call('modprobe snd-hda-intel', shell=True)
    #check_call('modprobe snd-pcm-oss', shell=True)
    #check_call('adduser radman audio', shell=True)

    # Create some misc directories under the VM; helps with testing...
    for dirname in ['/data/radio_retail/stores_software/data',
                    '/data/radio_retail/progs/loader/prerec_error',
                    '/data/radio_retail/stores_software/data_maintenance_error']:
        if not isdir(dirname):
            makedirs(dirname)
        check_call('chown radman:radman %s' % dirname, shell=True)

    check_call('aumix -v 100 -w 100', shell=True)

    # Get some music MP3s to test with:
    check_call('rsync -va --progress /vagrant/hax_mp3s/ /data/radio_retail/stores_software/data/', shell=True)

    # Update music volume level
    check_call("""su postgres -c 'psql -d schedule -c "UPDATE tblstore SET intmusicvolume = 200"'""", shell=True)

    ## HACK:
    #check_call('gdebi -n /vagrant/tmp_del_me/rrmedia-maintenance2_2.0.1_amd64.deb', shell=True)

    #if not isdir('/data/radio_retail/stores_software/data_maintenance_error'):
        #makedirs('/data/radio_retail/stores_software/data_maintenance_error')
    #check_call('chown radman:radman /data/radio_retail/stores_software/data_maintenance_error', shell=True)

    #assert 0

    #old_cwd = getcwd()
    #chdir('/data/radio_retail/progs/player')
    #check_call('su radman -c ./player &', shell=True)
    #sleep(20)
    #check_call('killall player', shell=True)
    #chdir(old_cwd)
    check_call("""/etc/init.d/rrplayer8 restart""", shell=True)


def get_deb_filename(cfg: 'Dict[str,Any]') -> str:
    """Get the name of the deb file to be built for this project.

    Args:
        cfg (Dict[str,Any])- configuration loaded from project_settings.yaml

    Returns:
        str: Debian package filename,
             eg: "rrdavid-demo-rest-mq-api_0.1.0_all.deb"

    """
    pkg_name = cfg['package']['name']
    pkg_ver = cfg['package']['version']
    pkg_fname = '%s_%s_amd64.deb' % (pkg_name, pkg_ver)
    return pkg_fname


def _get_fpm_bin_path():
    return '/root/.rbenv/versions/' + RUBY_VERSION + '/bin/fpm'


def _install_newer_ruby_version():

    # Install rbenv, to help us manage more recent Ruby versions:
    if not isfile('/usr/bin/rbenv'):
        check_call(['apt-get', 'install', '-y', '--force-yes', 'rbenv'])

    output = check_output(['rbenv', 'root']).strip().decode()
    plugins_dir = output + "/plugins"
    if not isdir(plugins_dir):
        print("Creating rbenv plugins directory %r" % plugins_dir)
        makedirs(plugins_dir)

    # Install 'git' utility if it's not already present:
    if not isfile('/usr/bin/git'):
        check_call(['apt', 'install', '-y', '--force-yes', 'git'])

    # Use git to clone the latest version of the plugin source code:
    ruby_build_plugin_dir = plugins_dir + '/ruby-build'
    if not isdir(ruby_build_plugin_dir):
        check_call(['git', 'clone', 'https://github.com/rbenv/ruby-build.git', ruby_build_plugin_dir])

    # Use this command to list the installable Ruby versions:
    #check_call(['rbenv', 'install', '--list'])

    # For now we just hardcode the Ruby version to install, in our
    # RUBY_VERSION global variable.

    ruby_install_dir='/root/.rbenv/versions/' + RUBY_VERSION
    if not isdir(ruby_install_dir):
        check_call(['rbenv', 'install', RUBY_VERSION])

    # Set PATH environment variable so that rbenv shims come first:
    os.environ['PATH'] = '/root/.rbenv/shims:' + os.environ['PATH']

    output = check_output(['ruby', '--version']).decode()
    assert output.startswith('ruby ' + RUBY_VERSION)


def _install_fpm():
    # Quit early if FPM is already installed:
    fpm_bin_path = _get_fpm_bin_path()

    # Set RBENV_VERSION environment variable so that we start using the newer
    # python version:
    os.environ['RBENV_VERSION'] = RUBY_VERSION

    if isfile(fpm_bin_path):
        return

    # Install and configure newer ruby interpreter version:
    _install_newer_ruby_version()

    # Attempt to use gem to install fpm:
    check_call(['gem', 'install', 'fpm'])


def run_vm_box_logic() -> None:
    """Run testing logic under a vagrant VM.

    Run our tests, deb-building, etc.

    This logic is run within the VM, after the VM has been properly
    initialized.

    """
    # Change into the project directory:
    chdir('/vagrant')

    # Load config settings:
    cfg = load_project_settings()

    # Tell apt-get and dpkg to run in non-interactive mode. ie, we
    # don't want user prompts/etc to confirm or configure package
    # installations.
    os.environ["DEBIAN_FRONTEND"] = "noninteractive"

    # Remove a deb installation file that might have errors from a recent edit,
    # and which might cause further problems during testing over here...
    pkg_name = cfg['package']['name']
    postinst_script = '/var/lib/dpkg/info/%s.postinst' % pkg_name
    if isfile(postinst_script):
        remove(postinst_script)

    # Set timezone to make this test VM a bit more like the prod
    # servers:
    flagfile = "/tmp/.tz-data-configured"
    if not isfile(flagfile):
        vm_tzname = cfg['test_vm_settings']['timezone_name']
        with open('/etc/timezone', 'w') as tz_file:
            tz_file.write("%s\n" % vm_tzname)
        check_call(['dpkg-reconfigure', 'tzdata'])
        with open(flagfile, 'w'):
            pass

    # Update sources.list within the VM if we haven't yet done that:
    flagfile = "/tmp/.apt-get-update-ran"
    if not isfile(flagfile):
        deb_url = cfg['test_vm_settings']['deb_mirror_urls']['debian']
        deb_release = cfg['test_vm_settings']['debian_release_name']
        ff_url = cfg['test_vm_settings']['deb_mirror_urls']['fullfacing']
        with open('/etc/apt/sources.list', 'w') as sources_file:
            sources_file.write("""deb %s %s main contrib non-free
deb %s testing radio-retail
""" % (deb_url, deb_release, ff_url))
        check_call(['apt-get', 'update'])
        with open(flagfile, 'w'):
            pass

    # A fix for a ruby ssl issue:
    flagfile = "/tmp/.lib-ssl-upgraded"
    if not isfile(flagfile):
        check_call(['apt-get', 'install', '-y', '--force-yes', 'libssl1.0.0|libssl1.0.2', 'libssl-dev'])
        with open(flagfile, 'w'):
            pass

    ## Run apt-get dist-upgrade within the VM if we haven't done that yet.
    #flagfile = "/tmp/.apt-get-dist-upgrade-ran"
    #if not isfile(flagfile):
        ##check_call(['apt-get', 'remove', '-y', 'apt-listchanges'])  # So we don't get some unwanted prompt during the dist-upgrade
        ##check_call(['apt-get', '-y', 'dist-upgrade'])
        #check_call(['apt-get', 'install', 'dist-upgrade'])
        #with open(flagfile, 'w'):
            #pass

    # Make sure that rsync is installed:
    if not isfile('/usr/bin/rsync'):
        check_call(['apt-get', 'install', '-y', 'rsync'])

    # Copy all our source code/etc, over to a temporary directory for
    # building, testing/ etc
    check_call(['rsync', '-va', '/vagrant/',
                '--exclude=/old/',
                '/tmp/vagrant_vm_test/'])

    # Change over to that directory, where we can now make gratuitous
    # changes without affecting the original project directory:
    chdir('/tmp/vagrant_vm_test')

    # Install logic that we use for building deb packages:
    if not isfile('/usr/bin/gem'):
        check_call(['apt-get', 'install', '-y', '--force-yes', 'ruby'])

    if not isfile('/usr/share/doc/ruby-dev/changelog.gz'):
        check_call(['apt-get', 'install', '-y', '--force-yes', 'ruby-dev'])

    # Install the fpm utility if not already present.
    _install_fpm()

    # Build the debian package:
    build_deb_installer(cfg)

    # Install the debian package:
    if not isfile("/usr/bin/gdebi"):
        check_call(['apt-get', 'install', '-y', '--force-yes', 'gdebi-core'])
    deb_fname = get_deb_filename(cfg)
    call(['gdebi', '-n', deb_fname])

    # Do various QA tests after the deb package has been installed. This would
    # be mainly functional/integration tests against the already-running
    # services.
    run_qa_tests_after_pkg_install(cfg)

    # At the very end: copy the debian package over to the /vagrant directory,
    # so that the developer immediately sees the new deb file:
    copy2(deb_fname, "/vagrant/%s" % deb_fname)
    print('NOTICE: Your new deb file is ready: ' + deb_fname)


def main() -> None:
    """Main logic.

    Only gets run if test.py is run as a script, rather than imported as a
    module.

    Raises:
        Exception: If the command provided on the command-line is unknown.

    """
    cmd = get_cmd_from_args()
    if cmd == "":
        run_dev_box_logic()
    elif cmd == "run_under_vm":
        run_vm_box_logic()
    else:
        raise Exception('Unknown command: %r' % cmd)


# Only run the main logic if test.py is being run as a script, as opposed to
# eg: being imported as a module.
if __name__ == '__main__':
    main()
