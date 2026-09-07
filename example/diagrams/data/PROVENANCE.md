# Where the corpora come from, and under what licence

Every parsed corpus in this directory derives from a public archive. The archive is not in
git; the commands below fetch it, and `../parsers/` and `make_corpus.py` turn it into the
`*.txt` files the test reads. Hashes are of the archive as fetched on the date given.

## WikiPathways, `hs.txt` (305 diagrams)

    curl -O https://data.wikipathways.org/20260810/gpml/wikipathways-20260810-gpml-Homo_sapiens.zip

Release 20260810, 1,016 GPML files, 10,824,119 bytes,
sha256 `78ebee9adc391c60a326f4f8650be21b620fd5907911c06ba40387209307fa87`.

Licence CC0 1.0. WikiPathways terms: "WikiPathways content is covered by the Creative
Commons CC0 Waiver, which states that you are free to share (copy, distribute and
transmit) and remix (adapt) the work." Attribution is not required and is given anyway:
Agrawal, Balci, Hanspers et al., WikiPathways 2024, *Nucleic Acids Research* 52(D1),
2024.

## Reactome, `sbgn.txt` (248 diagrams)

    curl -O https://reactome.org/download/current/homo_sapiens.sbgn.tar.gz

Release 97 (30 June 2026), 1,382 SBGN-ML files, 4,686,878 bytes,
sha256 `4745cfce27b80fe4843d06bf7b2b88fd1e35c62ff241074afd4b71f619db2acd`.

Licence CC0. Reactome's licence page, section 1(c): "All data in the Reactome database
and files derived from that data are licensed under the Creative Commons Public Domain
Dedication (CC0). User may copy, modify, and distribute these data, even for commercial
purposes, without asking for permission." The CC BY 4.0 clause of that page covers the
hand-drawn pathway illustrations and branding, not these exports. Cited as Fabregat et
al., *Bioinformatics* 34(7), 2018.

## BPMN Academic Initiative, `bpmn.txt`, `bpmnr.txt`, `bpmn41.txt`

    # Zenodo record 3758705, file BPMAI-29-10-2019.tar.gz, saved here as bpmai.tar.gz

29,810 models, 387,701,164 bytes, md5 `e53aca82c92eb0e8f9de33f746df6027` (the record
publishes md5; sha256 `7c40fe0c13f78943f1600f09203c30d83c41fd6c71b81902367bf057a73f947a`
is ours).

Licence **CC BY 3.0 Unported**, as stated on the record. Redistribution of derived files
is permitted with attribution, which is:

> Mathias Weske, Gero Decker, Marlon Dumas, Marcello La Rosa, Jan Mendling, Hajo A.
> Reijers. Model Collection of the Business Process Management Academic Initiative,
> version BPMAI-29-10-2019. Zenodo, 2020. doi:10.5281/zenodo.3758705. Licensed CC BY 3.0.
> Changes: the models were parsed to node coordinates, box sizes and edge lists; labels,
> model names and every other field were dropped.

This is **not** SAP Signavio Academic Models (Zenodo 7012043, licence Other
(Non-Commercial), "no rights to make derivative works of the Model Collection is
granted"). That is a later and larger collection from the same initiative's successor
workspace, and nothing here derives from it.

The derived files carry the model id, Signavio shape ids and geometry. The archive's
`.meta.json` files are not read (`parsers/parse_bpmn.py`), so no author, course or
timestamp field reaches this directory; the models are not attributable to individual
students from anything published here.

## HOLA formative drawings, `hola.txt` (136 drawings)

Parsed from the SVGs published at `https://data.graphlayout.net/HOLA/formative/`, the data
page of Kieffer, Dwyer, Marriott and Wybrow, HOLA: Human-like Orthogonal Network Layout,
*IEEE TVCG* 22(1), 2016. See `PROVENANCE_hola.md`.

**No licence is stated on that page or its site root, so no redistribution is granted.**
Permission was requested from the study's authors on 7 September 2026; this file records
the answer when it arrives. If it is no, `hola.txt` is removed and the fetch-and-parse
script stands in its place. The participant codes are the study's own published
pseudonyms.

## GD Collection, not redistributed

`../results/gdcoll_corpus.txt` is generated from a clone of
`github.com/hegetim/gd-collection` at commit `d113537` and is not committed: the
collection is CC BY-SA 4.0 and this repository is BSD-2-Clause. Only the medians in
`../results/gdcoll.md` are committed. Cite Mooney, Hegemann, Wolff, Wybrow and Purchase,
GD 2025, LIPIcs 357, artifact doi:10.4230/artifacts.25065.

## Personal data

`../results/hs_authors.csv` caches the first author of each WikiPathways pathway, which
the GPML file publishes, as pseudonyms `drawer_001` and up. The analysis in
`../results/drawers.py` needs only equality of drawer. Names, usernames and the IP
addresses WikiPathways records for unregistered editors are not kept here; re-run
`drawers.py` without the cache to fetch them from the source.
