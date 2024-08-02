import pandas as pd
from tabulate import tabulate
from pathlib import Path
from get_deps import deps_all

# TOP is tinyusb root dir
TOP = Path(__file__).parent.parent.resolve()


# -----------------------------------------
# Dependencies
# -----------------------------------------

def gen_deps_doc():
    deps_rst = Path(TOP) / "docs/reference/dependencies.rst"
    df = pd.DataFrame.from_dict(deps_all, orient='index', columns=['Repo', 'Commit', 'Required by'])
    df = df[['Repo', 'Commit', 'Required by']].sort_index()
    df = df.rename_axis("Local Path")

    outstr = f"""\
************
Dependencies
************

MCU low-level peripheral driver and external libraries for building TinyUSB examples

{tabulate(df, headers="keys", tablefmt='rst')}
"""

    with deps_rst.open('w') as f:
        f.write(outstr)


if __name__ == "__main__":
    gen_deps_doc()
