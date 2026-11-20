# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from analyzer import *
import drawsvg as dw

# COLORS = ["#FF1111", "#11FF11", "#1111FF", "#AA11AA"]

# COLORS = [
# 	"#012442",
# 	"#7E627B",
# 	"#E38664",
# 	"#FFA566",
# 	"#5BA0BF",
# 	"#587D71"
# ]

COLORS = ["#DAE8FC", "#FFE6CC", "#D5E8D4", "#F8CECC", "#FFF2CC", "#E1D5E7"]

COLORS_DARK = ["#6C8EBF", "#D6B656", "#82B366", "#B85450", "#D79B00", "#9673A6"]

COLORS_ALL = [
    ["#DAE8FC", "#6C8EBF"],
    ["#FFE6CC", "#D6B656"],
    ["#FFF2CC", "#D79B00"],
    ["#E1D5E7", "#9673A6"],
    ["#D5E8D4", "#82B366"],
    ["#F8CECC", "#B85450"],
]

BASELINE_HOT_FIX = True


class BarGraph:
    def __init__(self, scale):
        self.h = 1000
        self.w = 1400
        self.b_pad = -60
        self.l_pad = 100
        self.r_pad = 100

        self.d = dw.Drawing(self.w, self.h, id_prefix="pic", origin=(0, 0))

        self.scale = scale

        # add baseline:
        self.drawHorizontalLine(0, True)
        self.drawHorizontalLine(50, False)
        self.drawHorizontalLine(100, True)
        self.drawHorizontalLine(150, False)
        self.drawHorizontalLine(200, True)
        self.drawHorizontalLine(250, False)
        self.drawHorizontalLine(300, True)
        self.drawHorizontalLine(350, False)
        self.drawHorizontalLine(400, True)
        self.drawHorizontalLine(450, False)
        self.drawHorizontalLine(500, True)
        # self.drawHorizontalLine(400, False)

        # self.addBar("test", 100, "#5555CC", 150, 30)
        # self.addBar("valgrind", 150, "#CC6666", 83.2, 30)
        # self.addBar("valgrind 2", 180, "#22BB66", 200.2, 30)

        # self.addText("hmmm", 123)
        # self.addColorDot(123, 111, "#AAAAFF")
        # self.addColorDot(123, 116, "#991111", 0.3)

    def drawHorizontalLine(self, h: int, text: bool):
        scale_h = round(h / self.scale, 2)
        if text:
            self.d.append(
                dw.Text(
                    f"{scale_h}s",
                    font_size=20,
                    x=self.l_pad - 10,
                    y=self.h - h + self.b_pad + (0 if not BASELINE_HOT_FIX else 10),
                    text_anchor="end",
                    dominant_baseline="middle",
                    stroke="none",
                    fill="#555555",
                    # font_family="serif"
                )
            )
        self.d.append(
            dw.Line(
                self.l_pad,
                self.h - h + self.b_pad,
                self.w - self.r_pad,
                self.h - h + self.b_pad,
                stroke="#666666" if text else "#AAAAAA ",
            )
        )

    def addBar(self, name: str, pos: int, color, h: int, w: int = 30):
        tx = pos + w // 2
        ty = self.h + self.b_pad + 4

        self.d.append(
            dw.Text(
                name,
                font_size=10,
                x=tx,
                y=ty,
                text_anchor="end",
                dominant_baseline="hanging",
                transform=f"rotate(-40,{tx},{ty})",
            )
        )
        self.d.append(
            dw.Rectangle(
                pos,
                self.h + self.b_pad - h,
                w,
                h,
                fill=color,
                stroke="none",
                # fill_opacity=0.8
            )
        )
        self.d.append(
            dw.Text(
                f"{h}",
                font_size=10,
                x=pos + w // 2,
                y=self.h + self.b_pad - h - 5,
                text_anchor="middle",
            )
        )

    def addJustBar(self, pos: int, color, color_stroke, h: int, w: int = 30):
        self.d.append(
            dw.Rectangle(
                pos,
                self.h + self.b_pad - h,
                w,
                h,
                fill=color,
                # stroke="none",
                stroke=color_stroke,
                stroke_width=1,
                # fill_opacity=0.8
            )
        )

    def addText(self, name: str, pos: int):
        tx = pos
        ty = self.h + self.b_pad + 15

        self.d.append(
            dw.Text(
                name,
                font_size=24,
                x=tx,
                y=ty + (0 if not BASELINE_HOT_FIX else 12),
                text_anchor="middle",
                dominant_baseline="hanging",
                font_family="serif",
                font_weight="lighter",
            )
        )

    def addColorDot(self, pos: int, h: int, color, fill_opacity=1):
        self.d.append(
            dw.Circle(
                pos,
                self.h - h + self.b_pad,
                7,
                fill=color,
                stroke="none",
                fill_opacity=fill_opacity,
            )
        )

    def getPoints(self, n: int) -> list:
        assert n >= 2
        # margin = 70
        if n == 2:
            margin = 100

        l_m = self.l_pad
        r_m = self.r_pad
        w = self.w - l_m - r_m
        interval = w / (n)
        pos = l_m + interval / 2
        out = []
        for _ in range(n):
            out += [pos]
            pos += interval
        assert len(out) == n
        return out

    def save(self, f: str):
        self.d.save_svg(f)


def makeGraph(file, results, lang_base, test_id, case_id, scale):
    b = BarGraph(scale)

    points = b.getPoints(len(lang_base))

    for p, m in zip(points, lang_base):
        b.addText(HUMAN_NAME[m], p)

    for pos, lang in zip(points, lang_base):
        lang_id = LANG_ID[lang]
        for comp_id in range(COMPUTERS):
            comp_res = results[comp_id]
            assert comp_res.computer_id - 1 == comp_id

            test_res = comp_res.tests[test_id]

            lang_res = test_res.langs[lang_id]

            case_res = lang_res.cases[case_id]

            value = case_res.getAvg()

            w = 20
            # b.addColorDot(pos, value*scale, COLORS[comp_id])
            b.addJustBar(
                pos + (comp_id * w) - COMPUTERS * w / 2 - 1,
                COLORS_ALL[comp_id][0],
                COLORS_ALL[comp_id][1],
                value * scale,
                w - 4,
            )

    b.save(file)


def regenerateRJN(test_results):
    makeGraph(
        "svg_out/rjn_collatz_small.svg",
        test_results,
        ["rift", "java_no_jit", "node_no_jit"],
        COLLATZ,
        SMALL,
        500,
    )
    makeGraph(
        "svg_out/rjn_collatz_medium.svg",
        test_results,
        ["rift", "java_no_jit", "node_no_jit"],
        COLLATZ,
        MEDIUM,
        50,
    )
    makeGraph(
        "svg_out/rjn_collatz_large.svg",
        test_results,
        ["rift", "java_no_jit", "node_no_jit"],
        COLLATZ,
        LARGE,
        5,
    )

    makeGraph(
        "svg_out/rjn_fib_iter_small.svg",
        test_results,
        ["rift", "java_no_jit", "node_no_jit"],
        FIB_ITER,
        SMALL,
        300,
    )
    makeGraph(
        "svg_out/rjn_fib_iter_medium.svg",
        test_results,
        ["rift", "java_no_jit", "node_no_jit"],
        FIB_ITER,
        MEDIUM,
        30,
    )
    makeGraph(
        "svg_out/rjn_fib_iter_large.svg",
        test_results,
        ["rift", "java_no_jit", "node_no_jit"],
        FIB_ITER,
        LARGE,
        3,
    )

    makeGraph(
        "svg_out/rjn_fib_rec_small.svg",
        test_results,
        ["rift", "java_no_jit", "node_no_jit"],
        FIB_REC,
        SMALL,
        1500,
    )
    makeGraph(
        "svg_out/rjn_fib_rec_medium.svg",
        test_results,
        ["rift", "java_no_jit", "node_no_jit"],
        FIB_REC,
        MEDIUM,
        30,
    )
    makeGraph(
        "svg_out/rjn_fib_rec_large.svg",
        test_results,
        ["rift", "java_no_jit", "node_no_jit"],
        FIB_REC,
        LARGE,
        5,
    )


def regenerateRGV(test_results):
    makeGraph(
        "svg_out/rvg_collatz_small.svg",
        test_results,
        ["rift", "gdb", "valgrind"],
        COLLATZ,
        SMALL,
        500,
    )

    makeGraph(
        "svg_out/rvg_collatz_medium.svg",
        test_results,
        ["rift", "gdb", "valgrind"],
        COLLATZ,
        MEDIUM,
        100,
    )

    makeGraph(
        "svg_out/rvg_collatz_large.svg",
        test_results,
        ["rift", "gdb", "valgrind"],
        COLLATZ,
        LARGE,
        5,
    )

    makeGraph(
        "svg_out/rvg_fib_iter_small.svg",
        test_results,
        ["rift", "gdb", "valgrind"],
        FIB_ITER,
        SMALL,
        400,
    )

    makeGraph(
        "svg_out/rvg_fib_iter_medium.svg",
        test_results,
        ["rift", "gdb", "valgrind"],
        FIB_ITER,
        MEDIUM,
        100,
    )

    makeGraph(
        "svg_out/rvg_fib_iter_large.svg",
        test_results,
        ["rift", "gdb", "valgrind"],
        FIB_ITER,
        LARGE,
        10,
    )

    makeGraph(
        "svg_out/rvg_fib_rec_small.svg",
        test_results,
        ["rift", "gdb", "valgrind"],
        FIB_REC,
        SMALL,
        400,
    )

    makeGraph(
        "svg_out/rvg_fib_rec_medium.svg",
        test_results,
        ["rift", "gdb", "valgrind"],
        FIB_REC,
        MEDIUM,
        15,
    )

    makeGraph(
        "svg_out/rvg_fib_rec_large.svg",
        test_results,
        ["rift", "gdb", "valgrind"],
        FIB_REC,
        LARGE,
        2,
    )


def regeneratePrez(test_results):
    makeGraph(
        "svg_out/prez.svg",
        test_results,
        [
            "rift",
            "java",
            "java_no_jit",
            "node",
            "node_no_jit",
            "gdb",
            "valgrind",
            "python",
        ],
        COLLATZ,
        MEDIUM,
        25,
    )


if __name__ == "__main__":

    test_results = makeAllComputers()

    # regenerateRJN(test_results)
    # regenerateRGV(test_results)

    regeneratePrez(test_results)
