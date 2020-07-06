""" tlog tests """

import os
import ast
import stat
import time
import inspect
from shutil import copyfile
from systemd import journal

import pexpect
from pexpect.pxssh import pxssh


def journal_find_last():
    """ Find the last TLOG_REC journal entry """
    j = journal.Reader()
    j.seek_tail()
    while True:
        entry = j.get_previous()

        if '_COMM' in entry:
            matchfield = '_COMM'
        elif 'SYSLOG_IDENTIFIER' in entry:
            matchfield = 'SYSLOG_IDENTIFIER'
        else:
            continue

        if 'tlog' in entry[matchfield]:
            return entry
        elif not entry:
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


def check_recording_missing(shell, pattern, filename=None):
    """ Check that recording does not contain pattern """
    time.sleep(1)
    if filename is not None:
        cmd = 'tlog-play -g end -i {}'.format(filename)
    else:
        entry = journal_find_last()
        message = entry['MESSAGE']
        rec = ast.literal_eval(message)['rec']
        tlog_rec = 'TLOG_REC={}'.format(rec)
        cmd = 'tlog-play -g end -r journal -M {}'.format(tlog_rec)
    shell.sendline(cmd)
    out = shell.expect([pexpect.TIMEOUT, pattern], timeout=5)
    if out == 1:
        print('\ncheck_recording_missing found: {}'.format(pattern))
    assert out == 0


def ssh_pexpect(username, password, hostname, encoding='utf-8'):
    """ Setup an ssh connection to remote host """
    ssh = pxssh(options={
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


def mkrecording(shell, filename=None, sleep=3):
    """ Create a standard recording for tests to use """
    if filename is None:
        opts = '-w journal'
    else:
        opts = '-o {}'.format(filename)
    shell.sendline('tlog-rec {}'.format(opts))
    shell.sendline('id')
    shell.sendline('cat /etc/hosts')
    shell.sendline('exit')
