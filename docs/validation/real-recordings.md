# Real-recording cross-check

Independent estimator (numpy, same Hann/mean-removal/parabolic conventions) vs HumTrace on real files. The files have no mains-hum ground truth, so this checks decoding, estimator agreement and false-positive behavior only.

| File | Rate | Ch | Sec | Status | Frames compared | Max freq diff (Hz) | Max level diff (dB) | Frames with >=3 mains bins | Tracks |
|---|---|---|---|---|---|---|---|---|---|
| Google_Gnome.wav | 16000 | 1 | 55.6 | ok | 73 | 0.0000 | 0.000 | 0/217 | 89 |
| alice51_6ch.wav | 44100 | 6 | 58.3 | ok | 468 | 0.0000 | 0.000 | 0/936 | 304 |
| brooklyn_bridge.wav | 44100 | 2 | 1.8 | ok | 8 | 0.0000 | 0.000 | 0/8 | 0 |
| commercial_mono.wav | 8000 | 1 | 45.3 | ok | 88 | 0.0000 | 0.000 | 1/176 | 0 |
| commercial_stereo.wav | 44100 | 2 | 35.7 | ok | 192 | 0.0000 | 0.000 | 0/192 | 35 |
| en-US.wav | 8000 | 1 | 45.3 | ok | 88 | 0.0000 | 0.000 | 1/176 | 0 |
| hello.wav | 44100 | 1 | 0.5 | ok | 2 | 0.0000 | 0.000 | 0/2 | 0 |
| lo_apert.wav | 22050 | 1 | 1.1 | ok | 2 | 0.0000 | 0.000 | 0/2 | 0 |
| lo_apert2.wav | 22050 | 1 | 1.4 | ok | 3 | 0.0000 | 0.000 | 0/3 | 1 |
| lo_applause.wav | 11025 | 1 | 4.1 | ok | 11 | 0.0000 | 0.000 | 0/11 | 0 |
| lo_beam.wav | 11025 | 1 | 2.0 | ok | 5 | 0.0000 | 0.000 | 0/5 | 0 |
| lo_beam2.wav | 11025 | 1 | 3.3 | ok | 8 | 0.0000 | 0.000 | 5/8 | 7 |
| lo_cow.wav | 11025 | 1 | 1.7 | ok | 4 | 0.0000 | 0.000 | 2/4 | 0 |
| lo_curve.wav | 22050 | 1 | 3.1 | ok | 8 | 0.0000 | 0.000 | 0/8 | 6 |
| lo_drama.wav | 11025 | 1 | 7.4 | ok | 19 | 0.0000 | 0.000 | 2/19 | 18 |
| lo_explos.wav | 11025 | 1 | 2.2 | ok | 5 | 0.0000 | 0.000 | 0/5 | 0 |
| lo_falling.wav | 11025 | 1 | 3.9 | ok | 10 | 0.0000 | 0.000 | 10/10 | 10 |
| lo_glasses.wav | 11025 | 1 | 1.6 | ok | 4 | 0.0000 | 0.000 | 2/4 | 3 |
| lo_gong.wav | 11025 | 1 | 8.7 | ok | 23 | 0.0000 | 0.000 | 16/23 | 20 |
| lo_horse.wav | 11025 | 1 | 1.9 | ok | 5 | 0.0000 | 0.000 | 0/5 | 2 |
| lo_kling.wav | 11025 | 1 | 2.1 | ok | 5 | 0.0000 | 0.000 | 4/5 | 6 |
| lo_kongas.wav | 11025 | 1 | 2.5 | ok | 6 | 0.0000 | 0.000 | 0/6 | 2 |
| lo_laser.wav | 22050 | 1 | 0.2 | ok | 2 | 0.0000 | 0.000 | 0/2 | 0 |
| lo_left.wav | 22050 | 1 | 0.7 | ok | 3 | 0.0000 | 0.000 | 0/3 | 1 |
| lo_nature1.wav | 11025 | 1 | 3.8 | ok | 10 | 0.0000 | 0.000 | 0/10 | 3 |
| lo_nature2.wav | 11025 | 1 | 1.7 | ok | 4 | 0.0000 | 0.000 | 3/4 | 4 |
| lo_ok.wav | 11025 | 1 | 0.4 | ok | 2 | 0.0000 | 0.000 | 0/2 | 0 |
| lo_pluck.wav | 11025 | 1 | 1.3 | ok | 3 | 0.0000 | 0.000 | 1/3 | 3 |
| lo_roll.wav | 22050 | 1 | 5.5 | ok | 14 | 0.0000 | 0.000 | 0/14 | 3 |
| lo_romans.wav | 11025 | 1 | 4.1 | ok | 11 | 0.0000 | 0.000 | 4/11 | 8 |
| lo_soft.wav | 22050 | 1 | 4.2 | ok | 11 | 0.0000 | 0.000 | 0/11 | 8 |
| lo_space.wav | 22050 | 1 | 3.0 | ok | 8 | 0.0000 | 0.000 | 0/8 | 7 |
| lo_space2.wav | 22050 | 1 | 4.4 | ok | 11 | 0.0000 | 0.000 | 0/11 | 8 |
| lo_space3.wav | 22050 | 1 | 3.3 | ok | 8 | 0.0000 | 0.000 | 0/8 | 9 |
| lo_sparcle.wav | 11025 | 1 | 3.0 | ok | 7 | 0.0000 | 0.000 | 4/7 | 9 |
| lo_strom.wav | 11025 | 1 | 3.9 | ok | 10 | 0.0000 | 0.000 | 1/10 | 7 |
| lo_theetone.wav | 11025 | 1 | 2.2 | ok | 6 | 0.0000 | 0.000 | 4/6 | 7 |
| lo_top.wav | 22050 | 1 | 1.6 | ok | 4 | 0.0000 | 0.000 | 0/4 | 0 |
| lo_train.wav | 11025 | 1 | 2.6 | ok | 6 | 0.0000 | 0.000 | 0/6 | 3 |
| lo_untie.wav | 22050 | 1 | 4.3 | ok | 11 | 0.0000 | 0.000 | 0/11 | 12 |
| lo_ups.wav | 11025 | 1 | 1.3 | ok | 3 | 0.0000 | 0.000 | 0/3 | 0 |
| lo_wallewal.wav | 11025 | 1 | 4.7 | ok | 12 | 0.0000 | 0.000 | 2/12 | 9 |
| medical_conversation_2.wav | 16000 | 1 | 57.6 | ok | 75 | 0.0000 | 0.000 | 6/225 | 23 |
| multi.wav | 44100 | 2 | 5.2 | ok | 26 | 0.0000 | 0.000 | 0/26 | 1 |
| multi_es.wav | 44100 | 2 | 1.6 | ok | 8 | 0.0000 | 0.000 | 0/8 | 0 |
| podcast.wav | 44100 | 2 | 120.0 | ok | 130 | 0.0000 | 0.000 | 1/644 | 85 |

Problems: 0

## Source, scope and what this does and does not show

- 46 real WAV files, none committed (audio is not redistributed; see `real-recordings-manifest.json` for source, size, SHA-256): 11 speech/podcast recordings from the public Google Cloud sample bucket (`gs://cloud-samples-data/speech/`, incl. 8 kHz telephone, 16 kHz, 44.1 kHz stereo, a 6-channel 5.1 mix converted from AIFF, and 120 s of a podcast converted from FLAC) and 35 sound-effect files shipped with LibreOffice. Re-run: `python3 tools/real_recordings_check.py build/humtrace <dir> --markdown out.md`.
- **Result:** all files decode and analyze; HumTrace's strongest-peak frequency and level agree with an independent numpy implementation to the printed precision (0.0000 Hz, 0.000 dB over every compared frame); source SHA-256 matches.
- **Finding:** harmonic-bin "support" is non-specific. Harmonically rich non-mains sounds (a gong, falling-pitch effect, bells) show >=3 of 10 nominal 50/60 Hz harmonic bins above threshold in most frames, and speech/music produce many descriptive tracks (up to ~300 across 6 channels). The numbers are correct measurements but must not be read as hum detection; the UI/report wording is already non-attributive, and this is documented in the limitations.
- **Not covered:** no file here contains labeled mains hum or ENF content, so detection accuracy and false-negative rate on real hum remain unmeasured. The network policy of this environment blocked the hosts that serve hum/ENF datasets (Wikimedia Commons, Zenodo, Internet Archive, Freesound, SigPort, GitHub outside the session repository). Candidate sets to add once those hosts are allowed: the IEEE SP Cup 2016 ENF dataset (9 grids, audio + power reference, 50/60 Hz) and CC0/CC-BY "mains hum" recordings on Wikimedia Commons / Freesound.
