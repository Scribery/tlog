""" tlog environment configuration tests """
import os
import stat
import socket
import inspect

from shutil import copyfile
from tempfile import mkdtemp
import pytest

from misc import check_recording, mklogfile, ssh_pexpect
from config import TlogRecConfig, TlogRecSessionConfig


TLOG_REC_SESSION_PROG = "/usr/bin/tlog-rec-session"
TLOG_REC_PROG = "/usr/bin/tlog-rec"
SYSTEM_TLOG_REC_SESSION_CONF = "/etc/tlog/tlog-rec-session.conf"
SYSTEM_TLOG_REC_CONF = "/etc/tlog/tlog-rec.conf"
TMP_TLOG_REC_SESSION_CONF = "/tmp/tlog-rec-session.conf-test"
TMP_TLOG_REC_CONF = "/tmp/tlog-rec.conf-test"
TEXT_ENV_TLOG_REC_SESSION_CONF = "/tmp/tlog-rec-session.conf-text-test"
TEXT_ENV_TLOG_REC_CONF = "/tmp/tlog-rec.conf-text-test"
TLOG_TEST_LOCAL_USER = "tlitestlocaluser1"
TLOG_TEST_LOCAL_ADMIN = "tlitestlocaladmin1"


@pytest.fixture(scope="module")
def session_env_config_setup():
    """ Setup/teardown fixture applied to
    TestTlogRecSession tests"""
    conf_file = SYSTEM_TLOG_REC_SESSION_CONF
    backup_file = f"{conf_file}-orig"
    copyfile(conf_file, backup_file)
    yield session_env_config_setup
    # restore original configuration
    copyfile(backup_file, conf_file)
    os.remove(backup_file)


class TestTlogRecSession:
    """ tlog-rec-session tests """
    orig_hostname = socket.gethostname()
    tempdir = mkdtemp(prefix='/tmp/TestTlogRecSession.')
    user1 = TLOG_TEST_LOCAL_USER
    admin1 = TLOG_TEST_LOCAL_ADMIN
    os.chmod(tempdir, stat.S_IRWXU + stat.S_IRWXG + stat.S_IRWXO +
             stat.S_ISUID + stat.S_ISGID + stat.S_ISVTX)

    def test_conf_file_var(self, session_env_config_setup):
        """ Validate configuration settings are overwritten
        by TLOG_REC_SESSION_CONF_FILE variable
        """
        logfile = mklogfile(self.tempdir)
        msg = inspect.stack()[0][3]
        input_notice = "Test NOTICE"
        tmp_conf_file = TMP_TLOG_REC_SESSION_CONF

        # system wide journal configuration file
        sessionclass_system = TlogRecSessionConfig(writer="journal")
        sessionclass_system.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)

        # temporary configuration file to override with
        sessionclass_tmp = TlogRecSessionConfig(writer="file",
                                                file_writer_path=logfile,
                                                notice=input_notice)
        sessionclass_tmp.generate_config(tmp_conf_file)

        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline(f'export TLOG_REC_SESSION_CONF_FILE={tmp_conf_file}')
        shell.sendline(TLOG_REC_SESSION_PROG)
        # validate the notice override
        shell.expect(input_notice)
        shell.sendline(f'echo {msg}')
        shell.expect(msg)
        shell.sendline('exit')
        check_recording(shell, msg, logfile)
        shell.close()

    def test_conf_text_var(self, session_env_config_setup):
        """ Validate configuration settings are overwritten
        by TLOG_REC_SESSION_CONF_TEXT variable
        """
        logfile = mklogfile(self.tempdir)
        msg = inspect.stack()[0][3]
        tmp_notice_msg = "temp"
        tmp_conf_file = TMP_TLOG_REC_SESSION_CONF

        # system wide configuration file
        sessionclass_system = TlogRecSessionConfig(writer="journal")
        sessionclass_system.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)

        # temporary configuration file
        sessionclass_tmp = TlogRecSessionConfig(writer="file",
                                                file_writer_path=logfile,
                                                notice=tmp_notice_msg)
        sessionclass_tmp.generate_config(tmp_conf_file)

        # validate the notice override
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline(f'export TLOG_REC_SESSION_CONF_FILE={tmp_conf_file}')
        shell.sendline('export TLOG_REC_SESSION_CONF_TEXT='
                       '\'{"notice":"TEXT Notice"}\'')
        shell.sendline(TLOG_REC_SESSION_PROG)
        shell.expect("TEXT Notice")
        shell.sendline(f'echo {msg}')
        shell.expect(msg)
        shell.sendline('exit')
        shell.close()

    def test_conf_shell_var(self, session_env_config_setup):
        """ Validate the TLOG_REC_SESSION_SHELL variable
        is honored
        """
        msg = inspect.stack()[0][3]
        input_shell = "/bin/tcsh"

        sessionclass = TlogRecSessionConfig(writer="journal",
                                            shell='/bin/bash')
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)

        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline('export TLOG_REC_SESSION_CONF_TEXT='
                       '\'{"shell":"/bin/zsh"}\'')
        shell.sendline(f'export TLOG_REC_SESSION_SHELL={input_shell}')
        shell.sendline(TLOG_REC_SESSION_PROG)
        shell.expect("ATTENTION")
        shell.sendline("echo $SHELL")
        shell.expect(input_shell)
        shell.sendline(f'echo {msg}')
        shell.expect(msg)
        shell.sendline('exit')
        shell.close()


@pytest.fixture(scope="module")
def rec_env_config_setup():
    """ Setup/teardown fixture applied to
    TestTlogRec tests"""
    conf_file = SYSTEM_TLOG_REC_CONF
    backup_file = f"{conf_file}-orig"
    copyfile(conf_file, backup_file)
    yield rec_env_config_setup
    # restore original configuration
    copyfile(backup_file, conf_file)
    os.remove(backup_file)


class TestTlogRec:
    """ tlog-rec tests """
    orig_hostname = socket.gethostname()
    tempdir = mkdtemp(prefix='/tmp/TestTlogRec.')
    user1 = TLOG_TEST_LOCAL_USER
    admin1 = TLOG_TEST_LOCAL_ADMIN
    os.chmod(tempdir, stat.S_IRWXU + stat.S_IRWXG + stat.S_IRWXO +
             stat.S_ISUID + stat.S_ISGID + stat.S_ISVTX)

    def test_conf_file_var(self, rec_env_config_setup):
        """ Validate configuration settings are overwritten
        by TLOG_REC_CONF_FILE variable
        """
        logfile = mklogfile(self.tempdir)
        msg = inspect.stack()[0][3]
        tmp_conf_file = TMP_TLOG_REC_CONF

        # system wide configuration file
        recclass_system = TlogRecConfig(writer="journal")
        recclass_system.generate_config(SYSTEM_TLOG_REC_CONF)

        # temporary configuration file to override with
        recclass_tmp = TlogRecConfig(writer="file", file_writer_path=logfile)
        recclass_tmp.generate_config(tmp_conf_file)

        # validate the file writer override
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline(f'export TLOG_REC_CONF_FILE={tmp_conf_file}')
        shell.sendline(TLOG_REC_PROG + f"-c echo {msg}")
        check_recording(shell, msg, logfile)
        shell.close()

    def test_conf_text_var(self, rec_env_config_setup):
        """ Validate configuration settings are overwritten
        by TLOG_REC_CONF_TEXT variable
        """
        logfile = mklogfile(self.tempdir)
        msg = inspect.stack()[0][3]
        tmp_conf_file = TMP_TLOG_REC_CONF

        # system wide configuration file
        recclass_system = TlogRecConfig(writer="journal")
        recclass_system.generate_config(SYSTEM_TLOG_REC_CONF)

        # temporary configuration file
        recclass_tmp = TlogRecSessionConfig(writer="syslog")
        recclass_tmp.generate_config(tmp_conf_file)

        # validate the file writer override
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline(f'export TLOG_REC_CONF_FILE={tmp_conf_file}')
        shell.sendline('export TLOG_REC_CONF_TEXT=\'{"writer":"file"}\'')
        shell.sendline(TLOG_REC_PROG + f"-c echo {msg}")
        check_recording(shell, msg, logfile)
        shell.close()

    def test_conf_shell_var(self, session_env_config_setup):
        """ Validate the SHELL variable is honored when no
        command to record was specified
        """
        input_shell = "/bin/tcsh"

        recclass = TlogRecConfig(writer="journal")
        recclass.generate_config(SYSTEM_TLOG_REC_CONF)

        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        shell.sendline(f'export SHELL={input_shell}')
        shell.sendline(TLOG_REC_PROG)
        shell.sendline("echo $SHELL")
        shell.expect(input_shell)
        shell.sendline('exit')
        shell.close()
