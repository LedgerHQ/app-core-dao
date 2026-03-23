# ****************************************************************************
#    Ledger App Bitcoin
#    (c) 2023 Ledger SAS.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
# ****************************************************************************

########################################
#        Mandatory configuration       #
########################################

# Application version
APPVERSION_M = 0
APPVERSION_N = 1
APPVERSION_P = 1
APPVERSION = "$(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)"

APPDEVELOPPER="Ledger"
APPCOPYRIGHT="(c) 2025 Ledger"

VARIANT_PARAM = COIN
VARIANT_VALUES = core core_testnet

# simplify for tests
ifndef COIN
COIN=core_testnet
endif

# Enabling DEBUG flag will enable PRINTF and disable optimizations
#DEBUG = 10

APP_DESCRIPTION ="This app enables BTC timelocking with Core."

ifeq ($(COIN),core)
APPNAME ="Core"
BITCOIN_NETWORK =mainnet
DEFINES += CORE_MAINNET

else ifeq ($(COIN),core_testnet)
APPNAME ="Core Testnet"
BITCOIN_NETWORK =testnet
DEFINES += CORE_TESTNET

else ifeq ($(filter clean,$(MAKECMDGOALS)),)
$(error Unsupported COIN - use $(VARIANT_VALUES))
endif

APP_SOURCE_PATH += bitcoin_app_base/src src

# Application icons following guidelines:
# https://developers.ledger.com/docs/embedded-app/design-requirements/#device-icon
ICON_NANOX = icons/nanox_app_core.gif
ICON_NANOSP = icons/nanox_app_core.gif
ICON_STAX = icons/stax_app_core.gif
ICON_FLEX = icons/flex_app_core.gif
ICON_APEX_P = icons/apex_p_app_core.png

include bitcoin_app_base/Makefile
