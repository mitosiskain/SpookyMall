Add-Type -AssemblyName System.Drawing
$outputFile = Join-Path $PSScriptRoot 'T_GhostWriting_TestRGBA.png'
$bitmap = New-Object System.Drawing.Bitmap(768, 1024, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb))
$canvas = [System.Drawing.Graphics]::FromImage($bitmap)
$canvas.Clear([System.Drawing.Color]::Transparent)
$canvas.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$canvas.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 48, 15, 13))
$font = New-Object System.Drawing.Font('Segoe Print', 110, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel))
$pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(235, 48, 15, 13), 5)
try {
    $canvas.TranslateTransform(24, 0)
    $canvas.RotateTransform(-3)
    $canvas.DrawString('I AM', $font, $brush, 110, 130)
    $canvas.DrawString('STILL', $font, $brush, 82, 330)
    $canvas.DrawString('HERE', $font, $brush, 115, 540)
    $canvas.DrawBezier($pen, 90, 735, 270, 690, 430, 780, 630, 710)
    $canvas.DrawBezier($pen, 110, 757, 300, 728, 420, 793, 610, 740)
    $canvas.DrawLine($pen, 554, 157, 570, 260)
    $canvas.DrawLine($pen, 584, 151, 598, 264)
    $canvas.DrawLine($pen, 610, 153, 626, 264)
    $canvas.DrawLine($pen, 635, 148, 650, 260)
    $canvas.DrawLine($pen, 538, 236, 665, 176)
    $bitmap.Save($outputFile, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output $outputFile
} finally {
    $pen.Dispose()
    $font.Dispose()
    $brush.Dispose()
    $canvas.Dispose()
    $bitmap.Dispose()
}
