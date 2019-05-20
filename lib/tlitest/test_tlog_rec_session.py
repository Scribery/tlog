""" tlog tests """
import os
import stat
import time
import inspect
from tempfile import mkdtemp

import pytest

from misc import check_recording, mklogfile, mkcfgfile, \
                 ssh_pexpect, check_recording_missing, copyfile
from config import TlogRecConfig, TlogRecSessionConfig


SYSTEM_TLOG_REC_SESSION_CONF = "/etc/tlog/tlog-rec-session.conf"

class TestTlogRecSession:
    """ Test tlog-rec-session functionality """
    user = 'tlitestlocaluser2'
    tempdir = mkdtemp(prefix='/tmp/TestRecSession.')
    os.chmod(tempdir, stat.S_IRWXU + stat.S_IRWXG + stat.S_IRWXO +
             stat.S_ISUID + stat.S_ISGID + stat.S_ISVTX)

    @pytest.mark.tier1
    def test_session_record_to_file(self):
        """
        Check tlog-rec-session preserves session in a file
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile)
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        shell.sendline('echo {}'.format(myname))
        shell.sendline('exit')
        check_recording(shell, myname, logfile)
        shell.close()

    @pytest.mark.tier1
    def test_session_record_to_journal(self):
        """
        Check tlog-rec-session preserves session in journal
        """
        myname = inspect.stack()[0][3]
        sessionclass = TlogRecSessionConfig(writer="journal")
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        shell.sendline('echo {}'.format(myname))
        shell.sendline('exit')
        check_recording(shell, myname)
        shell.close()

    @pytest.mark.tier1
    def test_session_record_to_syslog(self):
        """
        Check tlog-rec-session preserves session via syslog
        """
        myname = inspect.stack()[0][3]
        sessionclass = TlogRecSessionConfig(writer="syslog")
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        shell.sendline('echo {}'.format(myname))
        shell.sendline('exit')
        check_recording(shell, myname)
        shell.close()

    def test_session_record_fast_input_with_latency(self):
        """
        Check tlog-rec-session caches data some time before logging
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            latency=15)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        for num in range(0, 200):
            shell.sendline('echo {}_{}'.format(myname, num))
        shell.sendline('exit')
        check_recording(shell, '{}_199'.format(myname), logfile)
        shell.close()

    def test_session_record_fast_input_with_payload(self):
        """
        Check tlog-rec limits output payload size
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            payload=128)
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        for num in range(0, 200):
            shell.sendline('echo {}_{}'.format(myname, num))
        shell.sendline('exit')
        check_recording(shell, '{}_199'.format(myname), logfile)
        shell.close()

    def test_session_record_fast_input_with_limit_rate(self):
        """
        Check tlog-rec-session records session with limit rate
        configured
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            limit_rate=10)
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        for num in range(0, 200):
            shell.sendline('echo {}_{}'.format(myname, num))
        shell.sendline('exit')
        check_recording(shell, '{}_199'.format(myname), logfile)
        shell.close()

    def test_session_record_fast_input_with_limit_burst(self):
        """
        Check tlog-rec-session allows limited burst of fast output
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            limit_rate=10,
                                            limit_burst=100)
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        for num in range(0, 200):
            shell.sendline('echo {}_{}'.format(myname, num))
        shell.sendline('exit')
        check_recording(shell, '{}_199'.format(myname), logfile)
        shell.close()

    def test_session_record_fast_input_with_limit_action_drop(self):
        """
        Check tlog-rec-session drops output when logging limit reached
        """
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            limit_rate=10,
                                            limit_action="drop")
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        shell.sendline('cat /usr/share/dict/linux.words')
        time.sleep(1)
        shell.sendline('exit')
        shell.close()
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        check_recording_missing(shell, 'Byronite', logfile)
        check_recording_missing(shell, 'zygote', logfile)

    def test_session_record_fast_input_with_limit_action_delay(self):
        """
        Check tlog-rec-session delays recording when logging limit reached
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            limit_rate=500,
                                            limit_action="delay")
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        for num in range(0, 200):
            shell.sendline('echo {}_{}'.format(myname, num))
        shell.sendline('exit')
        check_recording(shell, '{}_199'.format(myname), logfile)
        shell.close()

    def test_session_record_fast_input_with_limit_action_pass(self):
        """
        Check tlog-rec-session ignores logging limits
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            limit_rate=500,
                                            limit_action="pass")
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        for num in range(0, 200):
            shell.sendline('echo {}_{}'.format(myname, num))
        shell.sendline('exit')
        check_recording(shell, '{}_199'.format(myname), logfile)
        shell.close()

    def test_session_record_with_different_shell(self):
        """
        Check tlog-rec-session can specify different shell
        """
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(shell="/usr/bin/tcsh",
                                            writer="file",
                                            file_writer_path=logfile)
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        shell.sendline('echo $SHELL')
        check_recording(shell, '/usr/bin/tcsh', logfile)
        shell.sendline('exit')

    @classmethod
    def teardown_class(cls):
        """ Copy original conf file back into place """
        filename = SYSTEM_TLOG_REC_SESSION_CONF
        bkup = '{}.origtest'.format(filename)
        copyfile(bkup, filename)
