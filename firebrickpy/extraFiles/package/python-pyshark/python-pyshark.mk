################################################################################
#
# python-pyshark
#
################################################################################
PYTHON_PYSHARK_VERSION = 0.2.7
PYTHON_PYSHARK_SOURCE  = master.tar.gz
PYTHON_PYSHARK_SOURCE = pyshark-$(PYTHON_PYSHARK_VERSION).tar.gz
PYTHON_PYSHARK_SITE = https://drive.google.com/file/d/0B-nm5Bk9MslyNjk1YUJkb2trZkk/edit?usp=sharing
PYTHON_PYSHARK_SETUP_TYPE = distutils

$(eval $(python-package))

