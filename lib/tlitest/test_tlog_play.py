""" tlog tests """
import os
import sys
import ast
import stat
import time
from tempfile import mkdtemp

import pexpect

import pytest

from misc import ssh_pexpect, journal_find_last, \
                 mklogfile, mkrecording


class TestTlogPlay:
    """ Tests for tlog-play basic usage """
    user1 = 'tlitestlocaluser1'
    tempdir = mkdtemp(prefix='/tmp/TestTlogPlay.')
    os.chmod(tempdir, stat.S_IRWXU + stat.S_IRWXG + stat.S_IRWXO +
             stat.S_ISUID + stat.S_ISGID + stat.S_ISVTX)

    @pytest.mark.tier1
    def test_play_from_file(self):
        """
        Check tlog-play can playback session from file
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        mkrecording(shell, logfile)
        shell.sendline('tlog-play -i {}'.format(logfile))
        shell.expect('KNOWN BUGS')

    @pytest.mark.tier1
    def test_play_from_journal(self):
        """
        Check tlog-play can playback session from journal
        """
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        mkrecording(shell, sleep=4)
        shell.close()

        entry = journal_find_last()
        message = entry['MESSAGE']
        rec = ast.literal_eval(message)['rec']
        tlog_rec = 'TLOG_REC={}'.format(rec)
        cmd = 'tlog-play -r journal -M {}'.format(tlog_rec)
        shell2 = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell2.sendline(cmd)
        out = shell2.expect([pexpect.TIMEOUT, 'KNOWN BUGS'], timeout=10)
        assert out == 1

    def test_play_at_speed_x2(self):
        """
        Check tlog-play can playback session at 2x speed
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        mkrecording(shell, logfile, sleep=15)
        shell.close()

        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        time_start = time.time()
        opts = '-i {} --speed=2'.format(logfile)
        shell.sendline('tlog-play {}'.format(opts))
        shell.expect('KNOWN BUGS')
        time_stop = time.time()
        assert time_stop-time_start < 9

    def test_play_at_goto_points(self):
        """
        Check tlog-play can start playback session from goto points
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline('tlog-rec -o {}'.format(logfile))
        shell.sendline('echo start')
        time.sleep(5)
        shell.sendline('echo test_1')
        shell.expect('test_1')
        time.sleep(5)
        shell.sendline('echo test_2')
        shell.expect('test_2')
        time.sleep(5)
        shell.sendline('echo test_3')
        shell.expect('test_3')
        shell.sendline('echo end')
        shell.sendline('exit')
        shell.close()

        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        time_start = time.time()
        opts = '-i {} -g start'.format(logfile)
        shell.sendline('tlog-play {}'.format(opts))
        shell.expect('end')
        time_stop = time.time()
        assert time_stop-time_start > 15
        shell.close()

        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        time_start = time.time()
        opts = '-i {} -g end'.format(logfile)
        shell.sendline('tlog-play {}'.format(opts))
        shell.expect('end')
        time_stop = time.time()
        assert time_stop-time_start < 2
        shell.close()

        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        time_start = time.time()
        opts = '-i {} -g 11'.format(logfile)
        shell.sendline('tlog-play {}'.format(opts))
        shell.expect('end')
        time_stop = time.time()
        assert time_stop-time_start < 8
        shell.close()

    def test_play_follow_running_session(self):
        """
        Check tlog-play can follow a live running session
        """
        logfile = mklogfile(self.tempdir)
        shell1 = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell2 = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell2.logfile = sys.stdout
        shell1.sendline('tlog-rec -o {}'.format(logfile))
        time.sleep(1)
        shell2.sendline('tlog-play -i {} -f'.format(logfile))
        shell1.sendline('echo line1')
        shell2.expect('line1')
        time.sleep(2)
        shell1.sendline('echo line2')
        shell2.expect('line2')
        time.sleep(2)
        shell1.sendline('echo line3')
        shell2.expect('line3')
        shell1.close()
        shell2.close()


class TestTlogPlayControl:
    """ Tests for tlog-play running key controls usage """
    user1 = 'tlitestlocaluser1'
    tempdir = mkdtemp(prefix='/tmp/TestTlogPlay.')
    os.chmod(tempdir, stat.S_IRWXU + stat.S_IRWXG + stat.S_IRWXO +
             stat.S_ISUID + stat.S_ISGID + stat.S_ISVTX)
    ctl_log = mklogfile(tempdir)

    @classmethod
    def setup_class(cls):
        """ create sample recorded session for key control tests """
        shell = ssh_pexpect(cls.user1, 'Secret123', 'localhost')
        shell.sendline('tlog-rec -o {}'.format(cls.ctl_log))
        shell.sendline('echo start')
        time.sleep(10)
        shell.sendline('echo testime_stop')
        time.sleep(10)
        shell.sendline('echo test2')
        time.sleep(10)
        shell.sendline('echo test3')
        time.sleep(10)
        shell.sendline('echo end')
        shell.sendline('exit')
        shell.close()

    def test_play_key_ctl_goto_end(self):
        """
        Check tlog-play key control goes to end of session
        """
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        time_start = time.time()
        shell.sendline('tlog-play -i {}'.format(self.ctl_log))
        time.sleep(1)
        shell.sendline('G')
        shell.expect('end')
        time_stop = time.time()
        assert time_stop-time_start < 3
        shell.close()

    def test_play_key_ctl_next_packet(self):
        """
        Check tlog-play key control skips to next packet
        """
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        time_start = time.time()
        shell.sendline('tlog-play -i {}'.format(self.ctl_log))
        time.sleep(1)
        shell.sendline('.')
        time.sleep(1)
        shell.sendline('.')
        time.sleep(1)
        shell.sendline('.')
        time.sleep(1)
        shell.sendline('.')
        shell.expect('end')
        time_stop = time.time()
        assert time_stop-time_start < 5
        shell.close()

    def test_play_key_ctl_double_speed(self):
        """
        Check tlog-play key control doubles playback speed
        """
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        time_start = time.time()
        shell.sendline('tlog-play -i {}'.format(self.ctl_log))
        time.sleep(1)
        shell.sendline('}')
        time.sleep(1)
        shell.sendline('}')
        shell.expect('end')
        time_stop = time.time()
        assert time_stop-time_start < 15
        shell.close()

    def test_play_key_ctl_quit(self):
        """
        Check tlog-play key control quits playback
        """
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        time_start = time.time()
        shell.sendline('tlog-play -i {}'.format(self.ctl_log))
        time.sleep(1)
        shell.sendline('q')
        time_stop = time.time()
        assert time_stop-time_start < 2
        shell.close()

    def test_play_key_ctl_halve_speed(self):
        """
        Check tlog-play key control halves speed

        The double speed steps are there as necessary pre-req steps
        """
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        time_start = time.time()
        shell.sendline('tlog-play -i {}'.format(self.ctl_log))
        time.sleep(1)
        shell.sendline('}')
        time.sleep(1)
        shell.sendline('}')
        time.sleep(1)
        shell.sendline('{')
        time.sleep(1)
        shell.sendline('{')
        shell.expect('end', timeout=40)
        time_stop = time.time()
        assert time_stop-time_start > 30
        shell.close()
