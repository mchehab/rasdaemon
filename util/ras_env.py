# SPDX-License-Identifier: GPL-2.0

# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>


"""
Rasdaemon environment variable management.

Implements the same API contract as ras-env.c, reading from the
configuration file and setting environment variables.
"""

import logging
import os
import sys

from dataclasses import dataclass, field
from typing import Optional

from ras_config import Sqlite3ConnParms

logger = logging.getLogger(__name__)

# NOTE: keep it in sync with ras-env.c.


class RasDaemonEnv:
    MAX_LINE_LEN = 1024

    @staticmethod
    def ras_set_env(fname: str) -> int:
        """
        Parse a configuration file and set environment variables.

        Format:
            KEY=value

        Empty lines and lines starting with '#' or ';' are ignored.

        Spaces and tabs are allowed.

        Args:
            fname: Path to the configuration file.

        Returns:
            0 on success, -1 on error.
        """
        if not fname:
            logger.error("Failed to open config file: empty filename")
            return -1

        try:
            with open(fname, "r", encoding="utf-8") as fp:
                nenv = 0
                ln = 1

                for line in fp:
                    line = line.strip()

                    if not line or line[0] == "#" or line[0] == ";":
                        ln += 1
                        continue

                    if "=" not in line:
                        logger.warning(
                            "line %d: invalid line: %s", ln, line.strip("\n")
                        )
                        ln += 1
                        continue

                    key, value = line.split("=", 1)
                    key = key.strip()
                    value = value.strip().strip('"')

                    if not key:
                        logger.error("line %d: Empty key in config line", ln)
                        return -1

                    p = os.environ.get(key)
                    if p:
                        logger.info("Skipping %s=%s (already set to %s)",
                                    key, value, p)
                    else:
                        os.environ[key] = value
                        nenv += 1

                    ln += 1
        except (FileNotFoundError, PermissionError, OSError) as e:
            logger.error(f"Failed to open config file {fname}: repr({e})")
            return -1

        return 0


# NOTE: keep it in sync with misc/rasdaemon.env


class PostgresqlConnParms:
    def __init__(self):
        self.host = os.environ.get("RAS_PG_HOST", "")
        self.port = os.environ.get("RAS_PG_PORT", "5432")
        self.user = os.environ.get("RAS_PG_USER", "rasdaemon")
        self.password = os.environ.get("RAS_PG_PASSWORD", "")
        self.database = os.environ.get("RAS_PG_DATABASE", "rasdaemon")
        self.schema = os.environ.get("RAS_PG_SCHEMA", "rasdaemon")
        self.ssl_mode = os.environ.get("RAS_PG_SSL_MODE", "false")
        self.use_ssl = os.environ.get("RAS_PG_USE_SSL", "false")
        self.connect_timeout = os.environ.get("RAS_PG_CONNECT_TIMEOUT", "10")


class MysqlConnParms:
    def __init__(self):
        self.host = os.environ.get("RAS_MYSQL_HOST", "")
        self.port = os.environ.get("RAS_MYSQL_PORT", "3306")
        self.user = os.environ.get("RAS_MYSQL_USER", "rasdaemon")
        self.password = os.environ.get("RAS_MYSQL_PASSWORD", "")
        self.database = os.environ.get("RAS_MYSQL_DATABASE", "rasdaemon")
        self.socket = os.environ.get("RAS_MYSQL_SOCKET", "")
        self.use_ssl = os.environ.get("RAS_MYSQL_USE_SSL", "false")
        self.connect_timeout = os.environ.get("RAS_MYSQL_CONNECT_TIMEOUT", "10")


@dataclass
class RasdaemonConfig:
    """Rasdaemon configuration variables."""

    # Database backend
    db_backend: str = "sqlite3"

    # RASDAEMON_HOSTNAME is used while recording remote database events. The
    # reporting tool reads hostnames from those records, so it is intentionally
    # not part of this configuration object.

    # Per-database connection parameters
    pg_conn_parms: PostgresqlConnParms = field(default_factory=PostgresqlConnParms)
    mysql_conn_parms: MysqlConnParms = field(default_factory=MysqlConnParms)
    sqlite3_conn_parms: Sqlite3ConnParms = field(default_factory=Sqlite3ConnParms)

    # Page CE configuration
    page_ce_refresh_cycle: str = os.environ.get("PAGE_CE_REFRESH_CYCLE", "24h")
    page_ce_threshold: str = os.environ.get("PAGE_CE_THRESHOLD", "50")
    page_ce_action: str = os.environ.get("PAGE_CE_ACTION", "soft")

    # Row CE configuration
    row_ce_refresh_cycle: str = os.environ.get("ROW_CE_REFRESH_CYCLE", "24h")
    row_ce_threshold: str = os.environ.get("ROW_CE_THRESHOLD", "50")
    row_ce_action: str = os.environ.get("ROW_CE_ACTION", "off")

    # CPU isolation configuration
    cpu_isolation_enable: str = os.environ.get("CPU_ISOLATION_ENABLE", "no")
    cpu_ce_threshold: str = os.environ.get("CPU_CE_THRESHOLD", "18")
    cpu_isolation_cycle: str = os.environ.get("CPU_ISOLATION_CYCLE", "24h")
    cpu_isolation_limit: str = os.environ.get("CPU_ISOLATION_LIMIT", "10")

    # Event trigger configuration
    trigger_dir: str = os.environ.get("TRIGGER_DIR", "")
    mc_ce_trigger: str = os.environ.get("MC_CE_TRIGGER", "")
    mc_ue_trigger: str = os.environ.get("MC_UE_TRIGGER", "")
    aer_ce_trigger: str = os.environ.get("AER_CE_TRIGGER", "")
    aer_ue_trigger: str = os.environ.get("AER_UE_TRIGGER", "")

    # CE Statistic Threshold
    mc_ce_stat_threshold: int = int(os.environ.get("MC_CE_STAT_THRESHOLD", "2000"))

    # Poison page statistics
    poison_stat_threshold: int = int(os.environ.get("POISON_STAT_THRESHOLD", "102400"))

    # ERST
    erst_delete: int = int(os.environ.get("ERST_DELETE", "1"))

    def __post_init__(self):
        self.db_backend = os.environ.get("RASDAEMON_DB_BACKEND", "sqlite3")
