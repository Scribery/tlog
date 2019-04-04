""" tlog tests """

import os
import ast
import stat
import time
import socket
import inspect
from shutil import copyfile
from tempfile import mkdtemp
from systemd import journal

import pexpect
from pexpect import pxssh

import pytest

def journal_find_last():
    """ Find the last TLOG_REC journal entry """
    j = journal.Reader()
    j.seek_tail()
    while True:
        entry = j.get_previous()

        if '_COMM' not in entry:
            continue
        elif 'tlog-rec' in entry['_COMM']:
            return entry
        elif len(entry) == 0:
            raise ValueError('Did not find TLOG_REC entry in journal')

def check_journal(pattern):
    """ Check that last journal entry contains pattern """
    time.sleep(1)
    for _ in range(0, 10):
        entry = journal_find_last()
        message = entry['MESSAGE']
        out_txt = ast.literal_eval(message)['out_txt']
        if pattern in out_txt:
            break
        else:
            time.sleep(5)
    assert pattern in out_txt


def check_outfile(pattern, filename):
    """ Check that file contains pattern """
    time.sleep(1)
    for _ in range(0, 10):
        file1 = open(filename, 'r')
        content = file1.read()
        file1.close()
        if pattern in content:
            break
        else:
            time.sleep(5)
    assert pattern in content


def check_recording(shell, pattern, filename=None):
    """ Check that recording contains pattern """
    time.sleep(1)
    if filename is not None:
        cmd = 'tlog-play -i {}'.format(filename)
    else:
        entry = journal_find_last()
        message = entry['MESSAGE']
        rec = ast.literal_eval(message)['rec']
        tlog_rec = 'TLOG_REC={}'.format(rec)
        cmd = 'tlog-play -r journal -M {}'.format(tlog_rec)
    shell.sendline(cmd)
    out = shell.expect([pexpect.TIMEOUT, pattern], timeout=10)
    # print(shell.before)
    # print(shell.after)
    if out == 0:
        print('\ncheck_recording TIMEOUT')
    assert out == 1


def ssh_pexpect(username, password, hostname, encoding='utf-8'):
    """ Setup an ssh connection to remote host """
    ssh = pexpect.pxssh.pxssh(options={
        "StrictHostKeyChecking": "no",
        "UserKnownHostsFile": "/dev/null"
        }, encoding=encoding, codec_errors='replace')
    ssh.force_password = True
    ssh.login(hostname, username, password)
    ssh.sendline('echo loggedin')
    ssh.expect('loggedin')
    # ssh.logfile = sys.stdout
    return ssh


def mklogfile(directory, filename=None):
    """ Create temporary logfile """
    # if filename not given, we use the name of the calling frame
    if filename is None:
        filename = '{}.tlog'.format(inspect.stack()[1][3])
    logfile = '{}/{}'.format(directory, filename)
    if not os.path.isdir(directory):
        os.makedirs(directory)
        os.chmod(directory, stat.S_IRWXU + stat.S_IRWXG + stat.S_IRWXO +
                 stat.S_ISUID + stat.S_ISGID + stat.S_ISVTX)
    return logfile


def mkcfgfile(filename, content):
    """ Create config file from string content """
    bkup = '{}.origtest'.format(filename)
    content = '\n'.join([line.lstrip() for line in content.split('\n')])
    if os.path.isfile(filename) and not os.path.isfile(bkup):
        copyfile(filename, bkup)
    with open(filename, 'w') as out:
        out.write(content)


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

    def test_record_journal_tlog_fields(self):
        """
        Check that documented TLOG fields are added to
        journal messages
        """
        msgtext = 'test_tlog_fields'
        command = f'tlog-rec -w journal echo {msgtext}'
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline(command)
        # avoid race condition with reading from the journal
        time.sleep(5)

        entry = journal_find_last()
        message = entry['MESSAGE']

        # match the message to ensure we found the right message
        out_txt = ast.literal_eval(message)['out_txt']

        assert msgtext in out_txt
        tlog_fields = ['TLOG_USER', 'TLOG_SESSION', 'TLOG_REC', 'TLOG_ID']
        for field in tlog_fields:
            value = entry[field]
            assert value
        check_recording(shell, msgtext)
        shell.close()

    def test_record_journal_setting_priority(self):
        """
        Write and validate a journal message with a
        non-default priority
        """
        priority = 'err'
        expected_priority_num = 3
        msgtext = 'test_journal_priority'
        command = f'tlog-rec -w journal --journal-priority={priority} ' \
                  f'echo {msgtext}'

        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline(command)
        # avoid race condition with reading from the journal
        time.sleep(5)

        entry = journal_find_last()
        message = entry['MESSAGE']
        priority = entry['PRIORITY']

        # match the message to ensure we found the right message
        out_txt = ast.literal_eval(message)['out_txt']

        assert msgtext in out_txt
        priority_entry = entry['PRIORITY']
        assert priority_entry == expected_priority_num
        check_recording(shell, msgtext)
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

    def test_record_syslog_setting_priority_facility(self):
        """
        Write and validate a journal message with a
        non-default priority
        """
        priority = 'err'
        facility = 'auth'
        expected_priority_num = 3
        expected_facility_num = 4
        msgtext = 'test_syslog_priority_facility'
        command = f'tlog-rec -w syslog --syslog-priority={priority} ' \
                  f'--syslog-facility={facility} echo {msgtext}'

        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        print(command)
        shell.sendline(command)
        # avoid race condition with reading from the journal
        time.sleep(5)

        entry = journal_find_last()
        message = entry['MESSAGE']

        # match the message to ensure we found the right message
        out_txt = ast.literal_eval(message)['out_txt']

        priority_entry = entry['PRIORITY']
        facility_entry = entry['SYSLOG_FACILITY']

        assert msgtext in out_txt
        assert priority_entry == expected_priority_num
        assert facility_entry == expected_facility_num
        check_recording(shell, msgtext)
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
