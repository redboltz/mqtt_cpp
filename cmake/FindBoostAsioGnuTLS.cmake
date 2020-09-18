find_path(BoostAsioGnuTLS_INCLUDE_DIR NAMES boost/asio/gnutls.hpp)

mark_as_advanced(BoostAsioGnuTLS_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(BoostAsioGnuTLS
  REQUIRED_VARS
    BoostAsioGnuTLS_INCLUDE_DIR
  )

if(BoostAsioGnuTLS_FOUND)
  set(BoostAsioGnuTLS_INCLUDE_DIRS ${BoostAsioGnuTLS_INCLUDE_DIR})
endif()