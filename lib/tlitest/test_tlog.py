""" tlog tests """

import os
import ast
import stat
import time
import pytest
import pexpect
import inspect
from pexpect import pxssh
from systemd import journal
from tempfile import mkdtemp


def journal_find_last():
    j = journal.Reader()
    j.seek_tail()
    entry = j.get_previous()
    while('TLOG_REC' not in entry):
        entry = j.get_previous()
    return entry


def check_journal(pattern):
    time.sleep(1)
    entry = journal_find_last()
    message = entry['MESSAGE']
    out_txt = ast.literal_eval(message)['out_txt']
    if pattern in out_txt:
        return 0
    else:
        return 1


def check_outfile(pattern, filename):
    time.sleep(1)
    file1 = open(filename, 'r')
    content = file1.read()
    file1.close()
    if pattern in content:
        return 0
    else:
        return 1


def check_recording(shell, pattern, filename=None):
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
    s = pxssh.pxssh(options={
                    "StrictHostKeyChecking": "no",
                    "UserKnownHostsFile": "/dev/null"},
                    encoding=encoding, codec_errors='replace')
    s.force_password = True
    s.login(hostname, username, password)
    s.sendline('echo loggedin')
    s.expect('loggedin')
    # s.logfile = sys.stdout
    return s


def mklogfile(directory, filename=None):
    # if filename not given, we use the name of the calling frame
    if filename is None:
        filename = '{}.tlog'.format(inspect.stack()[1][3])
    logfile = '{}/{}'.format(directory, filename)
    if not os.path.isdir(directory):
        os.makedirs(directory)
        os.chmod(tempdir, stat.S_IRWXU + stat.S_IRWXG + stat.S_IRWXO +
                 stat.S_ISUID + stat.S_ISGID + stat.S_ISVTX)
    return logfile


class TestTlogRec(object):
    tempdir = mkdtemp(prefix='/tmp/TestTlogRec.')
    user1 = 'tlitestlocaluser1'
    admin1 = 'tlitestlocaladmin1'
    os.chmod(tempdir, stat.S_IRWXU + stat.S_IRWXG + stat.S_IRWXO +
             stat.S_ISUID + stat.S_ISGID + stat.S_ISVTX)

    def test_record_command_to_file(self):
        """
        As a user, record command output to a file
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline('tlog-rec -o {} whoami'.format(logfile))
        check_outfile('out_txt\":\"localuser1', logfile)
        check_recording(shell, 'localuser1', logfile)
        shell.close()

    def test_record_command_to_journal(self):
        """
        As a user, record command output to the systemd journal
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        reccmd = 'echo test_record_to_journal'
        shell.sendline('tlog-rec -w journal {}'.format(reccmd))
        check_journal('test_record_to_journal')
        check_recording(shell, 'test_record_to_journal')
        shell.close()

    def test_record_command_to_syslog(self):
        """
        As a user, record command output to syslog
        """
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        reccmd = 'echo test_record_to_syslog'
        shell.sendline('tlog-rec --writer=syslog {}'.format(reccmd))
        check_journal('test_record_to_syslog')
        check_recording(shell, 'test_record_to_syslog')
        shell.close()
