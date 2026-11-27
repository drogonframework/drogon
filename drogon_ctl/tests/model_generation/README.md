# Generated model regression

This standalone consumer test requires an installed Drogon with SQLite support,
its `drogon_ctl`, and Python 3 with the standard-library `sqlite3` module.
It does not need a database server. It generates models from real no-primary-key,
composite-key, manual-key, and automatic-key SQLite tables, compiles all generated
sources with unused parameters treated as errors, and checks ORM insert key
handling against the same database.

```sh
cmake -S drogon_ctl/tests/model_generation -B build-model-test \
  -DCMAKE_PREFIX_PATH=/path/to/drogon/install \
  -DDROGON_CTL=/path/to/drogon/install/bin/drogon_ctl
cmake --build build-model-test
ctest --test-dir build-model-test --output-on-failure
```

The default consumer uses C++17. Use `-DCMAKE_CXX_STANDARD=20` for C++20;
the installed Drogon must be compatible with the chosen language mode.
These are generated SQLite consumers; they do not claim live PostgreSQL/MySQL
coverage. The test is standalone so Python is not required for normal builds.
