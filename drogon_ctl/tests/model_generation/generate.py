"""Generate regression models using a real SQLite database and drogon_ctl."""
import json
from pathlib import Path
import sqlite3
import subprocess
import sys

ctl, output = sys.argv[1:]
output = Path(output).resolve()
output.mkdir(parents=True, exist_ok=True)
database = output / "models.db"
with sqlite3.connect(database) as connection:
    connection.executescript("""
        DROP TABLE IF EXISTS no_key;
        DROP TABLE IF EXISTS composite_key;
        DROP TABLE IF EXISTS manual_key;
        DROP TABLE IF EXISTS auto_key;
        CREATE TABLE no_key (value TEXT);
        CREATE TABLE composite_key (
            a TEXT, b TEXT, value TEXT, PRIMARY KEY (a, b));
        CREATE TABLE manual_key (id INT PRIMARY KEY, value TEXT);
        CREATE TABLE auto_key (
            id INTEGER PRIMARY KEY AUTOINCREMENT, value TEXT);
    """)
(output / "model.json").write_text(json.dumps({
    "rdbms": "sqlite3",
    "filename": str(database),
    "tables": ["no_key", "composite_key", "manual_key", "auto_key"],
}), encoding="utf-8")
subprocess.run([ctl, "create", "model", str(output), "-f"], check=True)
