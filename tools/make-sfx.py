"""Generate the original native game sound effects with the Python standard library."""

import math
import random
import struct
import wave
from pathlib import Path


RATE = 32000
OUTPUT = Path(__file__).resolve().parents[1] / "assets" / "sfx"


class Sound:
    def __init__(self, duration, seed):
        self.samples = [0.0] * round(RATE * duration)
        self.random = random.Random(seed)

    def layer(self, at, length, volume, attack, decay, source):
        start = round(at * RATE)
        for i in range(min(round(length * RATE), len(self.samples) - start)):
            t = i / RATE
            fade = min(1, t / attack, (length - t) / .012)
            self.samples[start + i] += volume * fade * math.exp(-t / decay) * source(t)

    def tone(self, at, length, volume, pitch, end=None, *, attack=.004, decay=.09, edge=.12):
        slope = math.log(end / pitch) / length if end else 0

        def oscillator(t):
            phase = math.tau * pitch * (math.expm1(slope * t) / slope if slope else t)
            return math.sin(phase + edge * math.sin(2.31 * phase))

        self.layer(at, length, volume, attack, decay, oscillator)

    def air(self, at, length, volume, low, high, *, attack=.008, decay=.065):
        # Two one-pole filters give the noise a defined frequency range.
        low_mix = -math.expm1(-math.tau * low / RATE)
        high_mix = -math.expm1(-math.tau * high / RATE)
        smoothed = baseline = 0.0

        def noise(_t):
            nonlocal smoothed, baseline
            smoothed += high_mix * (self.random.uniform(-1, 1) - smoothed)
            baseline += low_mix * (smoothed - baseline)
            return smoothed - baseline

        self.layer(at, length, volume, attack, decay, noise)

    def bell(self, at, pitch, volume, length=.34):
        self.tone(at, length, volume, pitch, decay=.43 * length)
        self.tone(at, length * .68, volume * .29, pitch * 2.76, decay=.19 * length, edge=0)
        self.tone(at, length * .41, volume * .12, pitch * 4.09, decay=.10 * length, edge=0)

    def sweep(self, at, length, volume, pitch, end):
        self.air(at, length, 1.45 * volume, 420, 3700, attack=.35 * length, decay=.65 * length)
        self.tone(at, length, .53 * volume, pitch, end, attack=.26 * length, decay=.57 * length, edge=.55)

    def impact(self, at, volume, pitch):
        self.tone(at, .21, volume, pitch, pitch * .43, decay=.055, edge=.44)
        self.air(at, .125, volume * 1.6, 580, 5400, attack=.002, decay=.024)

    def write(self, name, peak=.77):
        gain = 32767 * peak / max(map(abs, self.samples))
        with wave.open(str(OUTPUT / (name + ".wav")), "wb") as audio:
            audio.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
            audio.writeframes(struct.pack(f"<{len(self.samples)}h", *(round(s * gain) for s in self.samples)))


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)

    click = Sound(.078, 1)
    click.air(0, .031, .32, 850, 4700, attack=.001, decay=.009)
    click.tone(.001, .047, .41, 1660, 920, attack=.001, decay=.013, edge=.35)
    click.tone(.012, .064, .16, 2180, 2080, attack=.002, decay=.015)
    click.write("click", .68)

    chime = Sound(.36, 10)
    # A bright strike with two resonating metal tubes. The higher tube rings more softly.
    chime.tone(0, .36, .62, 1397, attack=.0015, decay=.085, edge=0)
    chime.tone(0, .26, .22, 3780, attack=.001, decay=.047, edge=0)
    chime.tone(0, .14, .09, 6310, attack=.0008, decay=.022, edge=0)
    chime.tone(.027, .32, .24, 1760, attack=.003, decay=.079, edge=0)
    chime.tone(.027, .19, .065, 4825, attack=.002, decay=.035, edge=0)
    chime.write("ui-chime", .68)

    for hit in (False, True):
        blade = Sound(.29 if hit else .19, 2)
        blade.sweep(0, .15, .74, 810, 230)
        if hit:
            blade.impact(.08, .74, 154)
            blade.bell(.081, 910, .18, .17)
        blade.write("blade-hit" if hit else "blade-miss")

    whip = Sound(.58, 3)
    whip.sweep(0, .20, .83, 200, 1390)
    whip.air(.15, .115, 1.3, 1150, 8100, attack=.001, decay=.021)
    whip.impact(.151, .74, 127)
    whip.sweep(.237, .32, .32, 510, 145)
    whip.write("wind-whip")

    phantom = Sound(1.08, 4)
    phantom.sweep(0, .51, .38, 186, 91)
    phantom.tone(.09, .83, .36, 94, 68, attack=.13, decay=.33)
    for start, pitch in ((.13, 719), (.26, 539), (.40, 427), (.55, 269)):
        phantom.bell(start, pitch, .38, .48)
    phantom.write("phantom-zone")

    moon = Sound(.72, 5)
    moon.sweep(0, .22, .79, 340, 1200)
    moon.sweep(.19, .25, .81, 1370, 315)
    moon.impact(.29, .72, 178)
    moon.bell(.30, 1140, .30, .39)
    moon.write("moon-slash")

    star = Sound(1.00, 6)
    star.sweep(0, .38, .37, 275, 1250)
    for start, pitch in ((.03, 494), (.105, 622), (.18, 740), (.255, 988)):
        star.bell(start, pitch, .34, .28)
    star.impact(.328, .64, 184)
    for pitch in (740, 988, 1245):
        star.bell(.35, pitch, .24, .59)
    star.write("starburst")

    spear = Sound(.68, 7)
    spear.sweep(0, .18, .76, 230, 1870)
    spear.air(.129, .09, 1, 1600, 6800, attack=.002, decay=.019)
    spear.impact(.139, .71, 214)
    spear.bell(.143, 1510, .35, .42)
    spear.bell(.22, 2030, .16, .36)
    spear.write("silver-spear")

    judgment = Sound(1.04, 8)
    for start in (0, .09, .175):
        judgment.air(start, .08, .70, 620, 3300, attack=.003, decay=.025)
        judgment.tone(start, .07, .34, 290, 139, decay=.027)
    judgment.impact(.275, .91, 104)
    judgment.bell(.28, 245, .49, .71)
    judgment.bell(.28, 348, .25, .67)
    judgment.write("judgment")

    void = Sound(1.19, 9)
    void.sweep(0, .39, .75, 73, 325)
    void.tone(.01, .37, .57, 83, 160, attack=.16, decay=.24, edge=.83)
    void.impact(.336, .96, 120)
    void.sweep(.343, .60, .69, 1030, 129)
    void.tone(.36, .76, .65, 77, 49, attack=.018, decay=.23, edge=.70)
    void.write("void-edge")


if __name__ == "__main__":
    main()
