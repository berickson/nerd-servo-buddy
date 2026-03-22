# Assets

Source artwork for web UI branding. Final images are base64-embedded in `src/web_page.h`.

## Rebuilding from source

Requires ImageMagick (`convert`, `identify`).

### 1. Trim whitespace
```bash
convert servo-large.png -fuzz 1% -trim +repage trimmed.png
```

### 2. Header logo (white on transparent, for dark mode page)
```bash
convert trimmed.png -channel RGB -negate -background transparent -flatten -resize 40x40 logo.png
```

### 3. Favicon (black on white, for browser tab)
```bash
convert trimmed.png -resize 32x32 -gravity center -background white -extent 32x32 favicon.png
convert favicon.png favicon.ico
```

### 4. Base64 encode
```bash
base64 -w0 favicon.ico    # use in <link rel="icon" type="image/x-icon" href="data:image/x-icon;base64,...">
base64 -w0 logo.png       # use in <img src="data:image/png;base64,...">
```

Paste the base64 output into the corresponding tags in `src/web_page.h`.

Clean up intermediate files when done.
