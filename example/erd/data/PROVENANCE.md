# Where this diagram comes from, and what was changed

A real MySQL Workbench model of a production schema: 44 tables, 65 foreign keys, and a diagram
layout a person maintained by hand across migrations. `ERD.png` is the current revision with
the tables the last migration added drawn in amber; `ERD_prev.png` is the revision before it,
36 tables. Between the two: ten tables added, two dropped. The pair is what an incremental
layouter has to reproduce -- the previous diagram, the new schema, and a human's accepted
answer for where the new tables went.

Anonymized before committing, structure untouched:

- Every table renamed to a letter, `A` through `AR`, in alphabetical order of its original
  name. `USER` kept its name; the external-participant table became `EXTERNAL_USER`. No
  original table name survives.
- Columns with no business meaning kept their names (`id`, `created`, `status`, ...). A column
  embedding a table's name follows that table's letter (`..._id`). The rest were renamed to
  neutral or enumerated (`f1`, `f2`, ...) names, including inside the SQL of the two views and
  the routine, index and constraint names, and `oldName` attributes.
- The schema name, the author field, migration file names listed in the routine, and the
  machine MAC address that Workbench's v1-style UUIDs embed as their node field were all
  replaced. Object cross-references still resolve because every id changed consistently.
- The `@db/data.db` sidecar (a Workbench cache) is not carried: a binary is where a missed
  name would hide. Workbench recreates it on open.

The geometry -- positions, sizes, the canvas -- is untouched. That is the point: the layout is
the ground truth, the names never mattered to it.

`graph_anon.json` is the same content flattened for programs: per revision, each table's
`[left, top, width, height]` and the FK edges between diagrammed tables, plus the list of
tables the migration added. `ERD.mwb` is the current revision only; the previous revision's
geometry lives in `graph_anon.json` under `prev`.

The source model is not public. The anonymization mapping is deliberately not committed
anywhere: without it, these letters do not lead back to the schema.
