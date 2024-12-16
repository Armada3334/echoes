import setuptools

with open("README.md", "r") as fh:
    long_description = fh.read()

setuptools.setup(
    # Here is the module name.
    name="ebrow",

    # version of the module
    # !!! GMB WARNING -  IT MUST MATCH THE VERSION IN pyproject.toml and maiwindow.py !!!!!!
    version = "0.1.83",   # PyPi
    # version = "99.2.0",    # TestPyPi

    # Name of Author
    author="Bertani Giuseppe Massimo",

    # your Email address
    author_email="gm_bertani@yahoo.it",

    # #Small Description about module
    description="Echoes Data Browser (Ebrow) is a data navigation and report generation tool for Echoes.",

    long_description=long_description,

    # Specifying that we are using markdown file for description
    long_description_content_type="text/markdown",

    # Any link to reach this module, ***if*** you have any webpage or github profile
    url="https://www.gabb.it/echoes/",
    packages=setuptools.find_packages(),

    # if module has dependencies i.e. if your package rely on other package at pypi.org
    # then you must add there, in order to download every requirement of package

    install_requires = [ # Optional
        "matplotlib>=3.7.1",
        "numpy>=1.24.3",
        "pandas>=2.0.0",
        "PyQt5>=5.15.7",
        "setuptools>=61.0",
        "mplcursors>=0.5.2",
        "python-dateutil>=2.8.2",
        "pyqtspinner>=2.0.0",
        "openpyxl>=3.1.2",
        "jinja2>=3.1.2",
	    "scipy>=1.11",
        "scikit-image>=0.2.2",
        "psutil==6.0.0"
    ],

    license="GPLv3",

    # classifiers like program is suitable for python3, just leave as it is.
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)",
        "Opebrating System :: OS Independent",
    ],
)
