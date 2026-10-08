# Research method and archival source handling

**Scope:** Repeatable method for future HumTrace research across software repositories, textbooks, journals, archived web pages, technical text files, and transcripts. Reviewed 2026-10-08.

## Source hierarchy

1. **Normative formats/standards:** current standards body or protocol publisher; record version and status.
2. **Peer-reviewed methods:** original paper and subsequent independent validation studies; note population, conditions, and limitations.
3. **Textbooks/reference books:** publisher/author catalog and specific edition; use as a synthesis/teaching source, not as proof a method works in HumTrace conditions.
4. **University material:** official course pages, lecture notes, repositories, and open courseware; record course/instructor/date and whether material is a textbook excerpt or instructor note.
5. **Software implementation:** authoritative upstream repository, tagged release, commit hash, source/license files, build/test workflow, and project documentation.
6. **Archived material:** Internet Archive/Wayback captures and institutional archives, cited as a snapshot with original URL, capture timestamp, and content identity. A capture is evidence of what that archive returned at a given capture time, not proof that its content was authoritative or first published then.
7. **Transcripts and mirrors:** discovery aids. Verify significant technical claims against the original recording, scan, paper, or publisher copy; preserve the transcript source, version, timestamps/pages, and known omissions.

Use reviews, blogs, forum posts, issue discussions, and search snippets to discover leads or user pain points. Do not use them as the sole support for algorithm definitions, license terms, safety claims, or scientific performance.

## Repository inspection checklist

For every relevant GitHub project, record repository URL, exact tag/commit, last meaningful release/update, license file/SPDX if present, build system, supported platforms/toolchains, audio formats, analysis methods, UI/accessibility model, test types/fixtures, CI, known limitations, dependencies and their licenses. Review test assertions and source paths rather than relying on README feature lists or stars. Record whether it is maintained upstream, a fork, an archived project, or a research artifact. A project being open source does not make its code compatible with HumTrace's future license or appropriate to copy.

Separate three outputs:

- **Reuse candidate:** code/library with pinned source and an explicit compatible license, dependency/security/build review, and a defined test boundary.
- **Design precedent:** useful interaction or architecture, independently implemented and credited; no code reuse implied.
- **Research lead:** claim requiring verification against standards or literature before entering a specification.

## Journal and textbook handling

Store full citations (authors, title, journal/book, edition/volume/pages, year, DOI/ISBN), publisher or author record, access route, and the exact chapter/section relevant to a HumTrace decision. Read abstracts only as discovery; retrieve the full paper or lawful preview before relying on methodological detail. Record paywall/preview limits. For each paper, note study data, sample count, acquisition conditions, evaluation split, error metrics, and whether it validates measurement, detection, source attribution, or only proposes a method. Never treat a cited forensic application as universal validation.

For books, identify editions because chapter structure and examples change. Do not store or reproduce copyrighted book chapters in the repository; keep bibliographic details, research notes, lawful public excerpts, and links. Publisher/author metadata confirms publication facts, not the correctness of every method.

## Archive/Wayback verification protocol

1. Start with the canonical organization/author/publisher URL. Search Internet Archive only if the live page has changed, disappeared, or an earlier version matters to a historical claim.
2. Query the Wayback CDX index for the exact URL (and redirect/asset variants if necessary). Record exact capture timestamp, original URL, status code, MIME type, digest and byte length when exposed. The Internet Archive `wayback` repository documents CDX index fields including timestamp, original URL, MIME type, status, digest and length. [1]
3. Open the timestamped capture and record its complete snapshot URL. Preserve screenshots/PDFs only when needed and permitted; retain the capture timestamp in every citation.
4. Check whether the page is a composite assembled from captures at different dates. Embedded images/scripts/styles may be missing or come from another capture. Research on Memento temporal coherence documents this issue. [2]
5. Compare the archived text with a canonical source, DOI/publisher metadata, repository history, or a second independent archive. Note differences; do not silently prefer the archived version.
6. Treat transcript/OCR errors, absent figures/tables, missing equations and stale links as uncertainty. Verify equations, parameter definitions, citations, and tables against the authoritative document before writing an implementation spec.
7. Label “capture observed on” separately from “published/updated on.” RFC 7089 defines the Memento framework and a datetime for time-based access to archived representations; that archive datetime is not the original publication date. [3]

The Library of Congress describes CDX as metadata/index information accompanying web archive resources and explains typical fields such as original URL and capture details. [4] Archive records are valuable historical evidence but are not a peer-review or provenance guarantee for the captured source itself.

## Research note template

Each research note should state: question/scope; review date; search method; sources and exact versions; findings; design implication; uncertainty/contradictions; decisions supported; decisions explicitly not supported; and bibliography with stable URLs/DOIs. Link each implementation spec to the relevant research note. Update time-sensitive licensing/platform claims before release, and mark a source as superseded when a newer edition/standard is adopted.

## Sources

1. Internet Archive, [`internetarchive/wayback` CDX server documentation](https://github.com/internetarchive/wayback/tree/master/wayback-cdx-server).
2. Ainsworth, S. G., Nelson, M. L., and Van de Sompel, H., [A Framework for Evaluation of Composite Memento Temporal Coherence](https://arxiv.org/abs/1402.0928), 2014.
3. Van de Sompel et al., [RFC 7089: HTTP Framework for Time-Based Access to Resource States (Memento)](https://www.rfc-editor.org/rfc/rfc7089), 2013.
4. Library of Congress, [CDX Internet Archive Index](https://www.loc.gov/preservation/digital/formats/fdd/fdd000590.shtml).
5. Internet Engineering Task Force, [About RFCs](https://www.ietf.org/process/rfcs/), including status and authoritative publication context.
