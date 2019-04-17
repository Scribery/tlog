""" tlog tests """
import os
import ast
import stat
import time
import socket
from tempfile import mkdtemp

import pexpect

import pytest

from misc import check_recording, ssh_pexpect, mklogfile, \
                 check_outfile, check_journal, mkcfgfile, \
                 journal_find_last


class TestTlogRec:
    """ tlog-rec tests """
    orig_hostname = socket.gethostname()
    tempdir = mkdtemp(prefix='/tmp/TestTlogRec.')
    user1 = 'tlitestlocaluser1'
    admin1 = 'tlitestlocaladmin1'
    os.chmod(tempdir, stat.S_IRWXU + stat.S_IRWXG + stat.S_IRWXO +
             stat.S_ISUID + stat.S_ISGID + stat.S_ISVTX)

    @pytest.mark.tier1
    def test_record_command_to_file(self):
        """
        Check tlog-rec preserves output when reording to file
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline('tlog-rec -o {} whoami'.format(logfile))
        check_outfile('out_txt\":\"{}'.format(self.user1), logfile)
        check_recording(shell, self.user1, logfile)
        shell.close()

    @pytest.mark.tier1
    def test_record_command_to_journal(self):
        """
        Check tlog-rec preserves output when recording to journal
        """
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        reccmd = 'echo test_record_to_journal'
        shell.sendline('tlog-rec -w journal {}'.format(reccmd))
        check_journal('test_record_to_journal')
        check_recording(shell, 'test_record_to_journal')
        shell.close()

    @pytest.mark.tier1
    def test_record_command_to_syslog(self):
        """
        Check tlog-rec preserves output when recording to syslog
        """
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        reccmd = 'echo test_record_to_syslog'
        shell.sendline('tlog-rec --writer=syslog {}'.format(reccmd))
        check_journal('test_record_to_syslog')
        check_recording(shell, 'test_record_to_syslog')
        shell.close()

    def test_record_interactive_session(self):
        """
        Check tlog-rec preserves activity during interactive
        session in recordings
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline('tlog-rec -o {}'.format(logfile))
        shell.sendline('whoami')
        shell.expect(self.user1)
        shell.sendline('sleep 2')
        shell.sendline('echo test123')
        shell.expect('test123')
        shell.sendline('echo test1123out>/tmp/pexpect.test1123out')
        check_outfile('test1123out', logfile)
        check_recording(shell, 'test1123out', logfile)
        shell.close()

    def test_record_binary_output(self):
        """
        Check tlog-rec preserves binary output in recordings
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        reccmd = 'cat /usr/bin/gzip'
        shell.sendline('tlog-rec -o {} {}'.format(logfile, reccmd))
        shell.expect(r'\u0000')
        check_recording(shell, r'\u0000', logfile)
        shell.close()

    def test_record_diff_char_sets(self):
        """
        Check tlog-rec preserves non-English I/O in recordings
        """
        logfile = '{}-ru_RU'.format(mklogfile(self.tempdir))
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline('tlog-rec -o {} /bin/bash'.format(logfile))
        shell.sendline('export LANG=ru_RU.utf8')
        shell.sendline('badcommand')
        shell.sendline('exit')
        check_outfile('найдена', logfile)
        check_recording(shell, 'найдена', logfile)
        shell.close()

        logfile = '{}-el_GR'.format(mklogfile(self.tempdir))
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline('tlog-rec -o {} /bin/bash'.format(logfile))
        shell.sendline('export LANG=el_GR.utf8')
        shell.sendline('badcommand')
        shell.sendline('exit')
        check_outfile('βρέθηκε', logfile)
        check_recording(shell, 'βρέθηκε', logfile)
        shell.close()

        logfile = '{}-en_US'.format(mklogfile(self.tempdir))
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline('tlog-rec -o {} /bin/bash'.format(logfile))
        shell.sendline('export LANG=en_US.utf8')
        shell.sendline('echo Watérmân')
        shell.sendline('exit')
        check_outfile('Watérmân', logfile)
        check_recording(shell, 'Watérmân', logfile)
        shell.expect('Watérmân')
        shell.close()

    def test_record_fast_input(self):
        """
        Check tlog-rec preserves fast flooded I/O in recordings
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline('tlog-rec -o {} /bin/bash'.format(logfile))
        for num in range(0, 2000):
            shell.sendline('echo test_{}'.format(num))
        shell.sendline('exit')
        for num in range(0, 2000, 100):
            check_recording(shell, 'test_{}'.format(num), logfile)
        shell.close()

    def test_record_as_unprivileged_user(self):
        """
        Check tlog-rec preserves unauthorized activity of
        unprivileged user in recordings
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')

        shell.sendline('tlog-rec -o {}'.format(logfile))
        shell.sendline('whoami')
        shell.expect(self.user1)
        shell.sendline('echo test1123out')
        shell.sendline('sleep 2')
        shell.sendline('ls -ltr /var/log/audit')
        check_recording(shell, 'test1123out', logfile)
        check_recording(shell, 'Permission denied', logfile)
        shell.sendline('exit')
        shell.close()

    def test_record_as_admin_user(self):
        """
        Check tlog-rec preserves sudo activity of admin user in
        recordings
        """
        logfile = mklogfile(self.tempdir)
        cfg = '''
        %wheel        ALL=(ALL)       NOPASSWD: ALL
        '''
        mkcfgfile('/etc/sudoers.d/01_wheel_nopass', cfg)
        shell = ssh_pexpect(self.admin1, 'Secret123', 'localhost')
        shell.sendline('tlog-rec -o {}'.format(logfile))
        shell.sendline('whoami')
        shell.expect(self.admin1)
        shell.sendline('sleep 2')
        shell.sendline('echo test1223')
        shell.expect('test1223')
        shell.sendline('sudo ls -ltr /var/log/audit')
        shell.expect('audit.log')
        shell.sendline('exit')
        check_outfile('test1223', logfile)
        check_recording(shell, 'test1223', logfile)
        shell.close()
        shell = ssh_pexpect(self.admin1, 'Secret123', 'localhost')
        check_recording(shell, 'audit.log', logfile)
        shell.close()

    # diable no-self-use in this function.  Otherwise pylint
    # complains that this method coule be a function.
    #
    # pylint: disable=no-self-use
    def test_record_from_different_hostnames(self):
        """
        Check tlog-rec reflects hostname changes in recordings

        This is to simulate receiving remote journal sessions
        """
        oldname = socket.gethostname()
        shell = pexpect.spawn('/bin/bash')
        for num in range(0, 3):
            newname = 'test{}-{}'.format(num, oldname)
            socket.sethostname(newname)
            open('/etc/hostname', 'w').write(newname)
            shell.sendline('hostname')
            shell.expect(newname)
            time.sleep(1)
            shell.sendline('tlog-rec -w journal whoami')
            time.sleep(1)
            shell.sendline('hostnamectl status')
            time.sleep(1)
            entry = journal_find_last()
            message = entry['MESSAGE']
            mhostname = ast.literal_eval(message)['host']
            assert mhostname == newname
            time.sleep(1)
        socket.sethostname(oldname)
        open('/etc/hostname', 'w').write(oldname)

    @classmethod
    def teardown_class(cls):
        """ teardown for TestTlogRec """
        socket.sethostname(cls.orig_hostname)
