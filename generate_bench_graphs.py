#!/usr/bin/env python3
"""Generate benchmark bar graphs for the Gecko blog post."""

import matplotlib.pyplot as plt
import matplotlib
import numpy as np

matplotlib.rcParams['font.size'] = 11
matplotlib.rcParams['figure.dpi'] = 150

# Color palette for parsers
COLORS = {
    'gcc -fsyntax-only': '#a6cee3',
    'gcc -O0':           '#6baed6',
    'clang -fsyntax-only': '#b2df8a',
    'clang -O0':         '#74c476',
    'YACC':              '#fdbf6f',
    'ElkHound':          '#ff7f00',
    'YAEP':              '#cab2d6',
    'Gecko':             '#e31a1c',
}

PROCESSORS = ['AMD 9900X', 'Apple M4', 'Intel 285K', 'Power10']


def make_clustered_bar(ax, parsers, data, title, ylabel='Time (seconds)'):
    """Create a clustered bar chart grouped by processor.

    parsers: list of parser names
    data: dict  parser -> [val_per_processor...]
    """
    n_procs = len(PROCESSORS)
    n_parsers = len(parsers)
    x = np.arange(n_procs)
    total_width = 0.82
    bar_w = total_width / n_parsers

    bars_collection = []
    for i, parser in enumerate(parsers):
        offset = (i - n_parsers / 2 + 0.5) * bar_w
        vals = data[parser]
        color = COLORS.get(parser, '#999999')
        bars = ax.bar(x + offset, vals, bar_w * 0.92, label=parser, color=color,
                      edgecolor='white', linewidth=0.5)
        bars_collection.append((bars, vals))

    # Put values on top of bars
    for bars, vals in bars_collection:
        for bar, val in zip(bars, vals):
            ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                    f'{val:.2f}', ha='center', va='bottom', fontsize=7, rotation=0)

    ax.set_xticks(x)
    ax.set_xticklabels(PROCESSORS)
    ax.set_ylabel(ylabel)
    ax.set_title(title, fontsize=13, fontweight='bold', pad=12)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.legend(loc='upper left', fontsize=8, ncol=2, framealpha=0.9)


def make_memory_bar(ax, parsers, mem_vals, title):
    """Horizontal bar chart for peak memory (single set of values)."""
    y = np.arange(len(parsers))
    colors = [COLORS.get(p, '#999999') for p in parsers]
    bars = ax.barh(y, mem_vals, color=colors, edgecolor='white', linewidth=0.5, height=0.6)
    for bar, val in zip(bars, mem_vals):
        ax.text(bar.get_width() + max(mem_vals) * 0.01, bar.get_y() + bar.get_height() / 2,
                f'{val:.0f}', ha='left', va='center', fontsize=9)
    ax.set_yticks(y)
    ax.set_yticklabels(parsers)
    ax.set_xlabel('Peak Memory (MB)')
    ax.set_title(title, fontsize=12, fontweight='bold', pad=10)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.invert_yaxis()


# ── Chart 1: C grammar, 1.5M lines (sieve) ──────────────────────────

parsers_sieve = ['gcc -fsyntax-only', 'clang -fsyntax-only',
                 'YACC', 'ElkHound', 'YAEP', 'Gecko']

data_sieve = {
    'gcc -fsyntax-only':   [2.93,  2.73,  2.58,  7.07],
    'clang -fsyntax-only': [1.95,  3.37,  2.96, 15.22],
    'YACC':                [0.68,  0.56,  0.58,  1.24],
    'ElkHound':            [1.27,  1.45,  1.14,  2.75],
    'YAEP':                [0.62,  0.77,  0.92,  1.90],
    'Gecko':               [0.71,  0.69,  0.74,  1.77],
}

mem_sieve = {
    'gcc -fsyntax-only': 1144, 'clang -fsyntax-only': 529,
    'YACC': 339, 'ElkHound': 127, 'YAEP': 1152, 'Gecko': 340,
}

fig1, (ax1a, ax1b) = plt.subplots(1, 2, figsize=(16, 6),
                                   gridspec_kw={'width_ratios': [3, 1.2]})
make_clustered_bar(ax1a, parsers_sieve, data_sieve,
                   'C Grammar: 1.5M Lines (100K Sieve Functions) -- Parse Time')
make_memory_bar(ax1b, parsers_sieve, [mem_sieve[p] for p in parsers_sieve],
                'Peak Memory (MB)')
fig1.tight_layout(w_pad=3)
fig1.savefig('bench_sieve.png', bbox_inches='tight')
plt.close(fig1)
print('wrote bench_sieve.png')

# ── Chart 2: C grammar, ~500K lines (whole old GCC) ─────────────────

parsers_gcc = ['gcc -fsyntax-only', 'clang -fsyntax-only',
               'YACC', 'ElkHound', 'YAEP', 'Gecko']

data_gcc = {
    'gcc -fsyntax-only':   [0.73, 0.97, 0.78, 1.81],
    'clang -fsyntax-only': [0.69, 1.03, 0.94, 4.14],
    'YACC':                [0.26, 0.29, 0.26, 0.59],
    'ElkHound':            [0.73, 0.84, 0.71, 1.73],
    'YAEP':                [0.66, 1.03, 0.71, 1.33],
    'Gecko':               [0.29, 0.30, 0.31, 0.68],
}

mem_gcc = {
    'gcc -fsyntax-only': 283, 'clang -fsyntax-only': 223,
    'YACC': 123, 'ElkHound': 127, 'YAEP': 471, 'Gecko': 124,
}

fig2, (ax2a, ax2b) = plt.subplots(1, 2, figsize=(16, 6),
                                   gridspec_kw={'width_ratios': [3, 1.2]})
make_clustered_bar(ax2a, parsers_gcc, data_gcc,
                   'C Grammar: ~500K Lines (Old GCC) -- Parse Time')
make_memory_bar(ax2b, parsers_gcc, [mem_gcc[p] for p in parsers_gcc],
                'Peak Memory (MB)')
fig2.tight_layout(w_pad=3)
fig2.savefig('bench_gcc.png', bbox_inches='tight')
plt.close(fig2)
print('wrote bench_gcc.png')

# ── Chart 3: Highly ambiguous grammar ────────────────────────────────

parsers_ambig = ['ElkHound', 'YAEP', 'Gecko']

data_ambig = {
    'ElkHound': [ 9.50, 15.65, 10.82, 13.82],
    'YAEP':     [ 5.53,  7.67,  5.77, 13.40],
    'Gecko':    [ 0.73,  0.80,  0.84,  1.59],
}

mem_ambig = {'ElkHound': 9.6, 'YAEP': 154, 'Gecko': 74}

fig3, (ax3a, ax3b) = plt.subplots(1, 2, figsize=(14, 5),
                                   gridspec_kw={'width_ratios': [3, 1.2]})
make_clustered_bar(ax3a, parsers_ambig, data_ambig,
                   'Highly Ambiguous Grammar (E=E+E|a, 200 ops) -- Parse Time')
make_memory_bar(ax3b, parsers_ambig, [mem_ambig[p] for p in parsers_ambig],
                'Parse-Only Memory (MB)')
fig3.tight_layout(w_pad=3)
fig3.savefig('bench_ambig.png', bbox_inches='tight')
plt.close(fig3)
print('wrote bench_ambig.png')

print('All graphs generated.')
