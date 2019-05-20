""" tlog remote recording tests """
import os

import pexpect

import pytest


class TestTlogPlayRemote:
    """ Test tlog-play for remotely recorded journal sessions """
    bootid = '6211b985bd1e49908726bd24cd514099'
    cases = {'good': '600-b0b9',
             'bad-timing': '72d-df52',
             'missing-entry': '786-11109'}

    @classmethod
    def setup_class(cls):
        """
        Import remotely recorded sessions for test cases to the
        journal
        """
        tpath = os.path.dirname(os.path.abspath(__file__))
        shell = pexpect.spawn('/bin/bash', cwd=os.getcwd())
        remdir = '/var/log/journal/remote'
        imported = '{}/test-journals-imported'.format(remdir)
        jremote = '/usr/lib/systemd/systemd-journal-remote'
        finish = 'Finishing after writing'
        if os.path.isfile(imported):
            return
        if not os.path.isdir(remdir):
            os.makedirs(remdir, mode=755, exist_ok=True)
        # for jtype in cls.cases.keys():
        for jtype, _ in cls.cases.items():
            jfile = '{}/journal-{}.txt'.format(tpath, jtype)
            jremfile = '{}/remote-{}.journal'.format(remdir, jtype)
            cmd = 'cat {}|{} -o {} -'.format(jfile, jremote, jremfile)
            shell.sendline(cmd)
            out = shell.expect([pexpect.TIMEOUT, finish], timeout=10)
            if out == 0:
                print('\nTestTlogPlayRemote setup TIMEOUT.')
                print('\nThere was a problem importing journals')
            assert out == 1
        open(imported, 'a').close()
        shell.close()

    @pytest.mark.tier1
    def test_play_remote_good_session(self):
        """
        Check tlog-play can playback remote session imported to
        journal
        """
        pattern = 'logout'
        shell = pexpect.spawn('/bin/bash')
        tlog_rec = 'TLOG_REC={}-{}'.format(self.bootid,
                                           self.cases['good'])
        cmd = 'tlog-play -r journal -M {} -g end'.format(tlog_rec)
        shell.sendline(cmd)
        out = shell.expect([pexpect.TIMEOUT, pattern], timeout=10)
        if out == 0:
            print('test_play_remote_good_session TIMEOUT')
        assert out == 1
        shell.close()

    def test_play_remote_bad_timing(self):
        """
        Check tlog-play fails on remote session with invalid timing
        """
        pattern = 'invalid "timing"'
        shell = pexpect.spawn('/bin/bash')
        tlog_rec = 'TLOG_REC={}-{}'.format(self.bootid,
                                           self.cases['bad-timing'])
        cmd = 'tlog-play -r journal -M {} -g end'.format(tlog_rec)
        shell.sendline(cmd)
        out = shell.expect([pexpect.TIMEOUT, pattern], timeout=10)
        if out == 0:
            print('test_play_remote_bad_timing TIMEOUT')
        assert out == 1
        shell.close()

    def test_play_remote_missing_entry(self):
        """
        Check tlog-play fails on remote session missing message
        """
        pattern = 'ID is out of order'
        shell = pexpect.spawn('/bin/bash')
        tlog_rec = 'TLOG_REC={}-{}'.format(self.bootid,
                                           self.cases['missing-entry'])
        cmd = 'tlog-play -r journal -M {} -g end'.format(tlog_rec)
        shell.sendline(cmd)
        out = shell.expect([pexpect.TIMEOUT, pattern], timeout=10)
        if out == 0:
            print('test_play_remote_missing_entry TIMEOUT')
        assert out == 1
        shell.close()
