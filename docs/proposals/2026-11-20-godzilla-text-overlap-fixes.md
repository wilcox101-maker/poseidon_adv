# GODZILLA order — B3: fix text overlaps on the watch faces

**From:** Claudius Maximus (overseer) · **To:** GODZILLA (node PSPE) · 2026-11-20
From the real renders of the merged faces (Classic, Command, Modular, Minimal on 1.85B / AMOLED-2.06 /
ES3C28P). **Priority: do this before E.** `godzilla_node/*` only, one draft PR, native-tested.

Two text elements collide on two faces. Modular and Minimal are clean — don't touch them.

## Overlap 1 — Classic: the "POSEIDON" wordmark overlaps the weekday sub-dial
On **both RTC boards**, the centered brand wordmark ("POSEIDON") is drawn in the upper-centre band that the
**weekday sub-dial** already occupies, so the two render on top of each other (reads as "POSEIDUR / THWED").
- **Fix (recommended):** **remove the centre brand wordmark from the Classic face.** The three sub-dials own the
  centre; the wordmark is decoration and there is no free band for it on a real dial. If you'd rather keep a
  maker's mark, place it on the lower cartouche between the dial centre and the moon aperture, sized and
  positioned from `gz_geom` so its bounding box clears every sub-dial and the moon window.
- **Native-test:** no sub-dial caption box intersects any other, and (if a wordmark is kept) its box intersects
  nothing. Put the sub-dial/wordmark bounding-box rects in a pure helper and assert disjoint on all boards.

## Overlap 2 — Command: the bottom ticker overlaps the bottom pods on the short board
On the **ES3C28P (240×320)** the bottom ticker ("CONTACTS 12  CH 6  PUB 3s") runs through the two **bottom pods**
(their footprints reach the bottom band). On the round 1.85B and the tall AMOLED there is room and it clears.
- **Fix:** make the ticker **footprint-aware** (same idea as B1). Compute the ticker's text band and the bottom
  pods' footprint band from `gz_geom`/`gz_cmdface.h`; if they would overlap, **drop the ticker on that board**
  (the pods already show the same fields) — or, if you can find a clear band, move it there. Prefer dropping it
  when there's no clear space; never draw it over a pod.
- **Native-test:** on all three boards the ticker's bounding box does not intersect any pod footprint; assert the
  ES3C28P case explicitly (ticker absent or provably clear).

## General rule (applies to any future face text)
Two text elements must never share pixels. Where a label's place depends on the board, derive its keep-out from
the one geometry source and native-test that the bounding boxes are disjoint — don't eyeball it.

Display change → note "ready to re-render"; Claudius re-renders on all three boards and confirms.
