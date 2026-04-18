@echo off
echo ========================================================
echo Starting local web server for the Robotic Hand Web App...
echo ========================================================
echo.
echo Please open Google Chrome or Microsoft Edge and navigate to:
echo http://localhost:8080
echo.
echo Keep this window open while you are using the controller.
echo Press Ctrl+C to stop the server when you are done.
echo.
python -m http.server 8080
pause
