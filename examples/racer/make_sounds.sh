#!/usr/bin/env bash
# Writes the Ring Racer race signals and crowd as 16-bit mono WAV files (22050 Hz):
#   beep.wav   - countdown beep, 660 Hz with a soft second harmonic.
#   go.wav     - start tone, 1320 Hz, longer.
#   lap.wav    - two-note bell chime for a completed lap.
#   finish.wav - rising four-note fanfare at the finish.
#   crowd.wav  - 8 s seamless loop of distant crowd murmur: thirty synthetic voices, each a pulse train through two
#                vowel formants that change every syllable, softened by a low-pass filter and freed of its DC offset.
set -euo pipefail
out="${1:-$(dirname "$0")/../../assets/racer/sounds}"
mkdir -p "$out"
export LC_ALL=C
# wav(seconds, awk-sample-program, file): the program sets v in [-1, 1] for sample n at time t; the last 30 ms fade out
# unless the program sets looped, then the samples are normalised to a peak of 0.8 and written after the RIFF header.
wav() {
	awk -v seconds="$1" "BEGIN {
		rate = 22050; count = int(seconds * rate); srand(5); pi = 3.14159265
		for (n = 0; n < count; ++n) { t = n / rate; v = 0; $2; if (!looped && seconds - t < 0.03) v *= (seconds - t) / 0.03
			s[n] = v; if (v > peak) peak = v; if (-v > peak) peak = -v }
		bytes = 2 * count
		printf \"RIFF\"; le(36 + bytes, 4); printf \"WAVEfmt \"; le(16, 4); le(1, 2); le(1, 2); le(rate, 4); le(2 * rate, 4); le(2, 2); le(16, 2)
		printf \"data\"; le(bytes, 4)
		for (n = 0; n < count; ++n) { q = int(s[n] / peak * 0.8 * 32767); le(q < 0 ? q + 65536 : q, 2) }
	}
	function le(value, size,   i) { for (i = 0; i < size; ++i) { printf \"%c\", value % 256; value = int(value / 256) } }" > "$3"
}
tone='a = t < 0.005 ? t / 0.005 : exp(-(t - 0.005) * decay); v = a * (sin(2 * pi * f * t) + 0.25 * sin(4 * pi * f * t))'
wav 0.25 "f = 660; decay = 6; $tone" "$out/beep.wav"
wav 0.8 "f = 1320; decay = 3; $tone" "$out/go.wav"
wav 0.9 'for (k = 0; k < 2; ++k) { s0 = k * 0.16; if (t >= s0) { u = t - s0; f = k ? 1319 : 988; v += exp(-u * 5) * (sin(2 * pi * f * u) + 0.3 * sin(2 * pi * 2.76 * f * u) * exp(-u * 12)) } }' "$out/lap.wav"
wav 1.6 'split("523 659 784 1047", notes, " ")
	for (k = 1; k <= 4; ++k) { s0 = (k - 1) * 0.15; if (t >= s0) { u = t - s0; held = k == 4 ? 1.1 : 0.15; a = u < 0.01 ? u / 0.01 : u < held ? 1 : exp(-(u - held) * 14)
		for (h = 1; h <= 6; ++h) { v += a * sin(2 * pi * h * notes[k] * u * (1 + 0.003 * sin(2 * pi * 5 * u))) / (h * h ^ 0.3) } } }' "$out/finish.wav"
# Crowd: per voice a pitch, a gain, and per syllable a vowel (two formant targets), a length and a pause. Formants glide
# towards their targets; each resonator is y = x + 2 r cos(w) y1 - r^2 y2. The last half second fades into the start.
wav 8.5 'if (n == 0) {
		split("730 1090 270 2290 530 1840 570 840 440 1020 300 870 660 1720 490 1350", vowel, " "); voices = 30; looped = 1
		for (k = 0; k < voices; ++k) { f0[k] = 95 + 130 * rand(); gain[k] = 0.4 + 0.6 * rand(); next_[k] = rand() * 0.3; f1[k] = 500; f2[k] = 1500 }
	}
	for (k = 0; k < voices; ++k) {
		if (t >= next_[k]) { choice = 2 * int(rand() * 8); t1[k] = vowel[choice + 1]; t2[k] = vowel[choice + 2]; start[k] = t
			len[k] = 0.1 + 0.2 * rand(); next_[k] = t + len[k] + 0.15 * rand() * rand() }
		f1[k] += (t1[k] - f1[k]) * 0.002; f2[k] += (t2[k] - f2[k]) * 0.002
		phase[k] += f0[k] * (1 + 0.05 * sin(2 * pi * 0.7 * t + k)) / rate; pulse = 0; if (phase[k] >= 1) { phase[k] -= 1; pulse = 1 }
		u = t - start[k]; env = u < len[k] ? sin(pi * u / len[k]) : 0
		w1 = 2 * pi * f1[k] / rate; r1 = exp(-pi * 90 / rate); y1 = pulse * env + 2 * r1 * cos(w1) * a1[k] - r1 * r1 * b1[k]; b1[k] = a1[k]; a1[k] = y1
		w2 = 2 * pi * f2[k] / rate; r2 = exp(-pi * 130 / rate); y2 = y1 + 2 * r2 * cos(w2) * a2[k] - r2 * r2 * b2[k]; b2[k] = a2[k]; a2[k] = y2
		v += gain[k] * (y1 + 0.5 * y2)
	}
	noiseLevel += (rand() - 0.5 - noiseLevel) * 0.05; v += 0.15 * noiseLevel
	low += (v - low) * 0.35; dc = low - lastLow + 0.995 * dc; lastLow = low; v = dc; s[n] = v
	if (n == count - 1) { loop = int(8.0 * rate); fade = count - loop
		for (m = 0; m < fade; ++m) { x = m / fade; s[m] = s[m] * x + s[loop + m] * (1 - x) }
		count = loop }' "$out/crowd.wav"
