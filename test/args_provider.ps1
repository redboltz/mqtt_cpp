Param( $command )
Start-Process -FilePath $command -ArgumentList $env:CTEST_ARGS -Wait
