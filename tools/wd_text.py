"""Host-side text → RGB for L0 demo banners (no device font)."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# Prefer common Linux TTFs; bitmap upscale is the portable fallback.
_TTF_CANDIDATES = (
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
)


def _truetype(size: int) -> ImageFont.ImageFont | None:
    for path in _TTF_CANDIDATES:
        if Path(path).is_file():
            try:
                return ImageFont.truetype(path, size=size)
            except OSError:
                continue
    return None


def fit_font(pixel_height: int) -> tuple[ImageFont.ImageFont, int]:
    """Return (font, scale). scale>1 means caller should nearest-upscale after draw."""
    # Leave padding; banner text ~55–70% of strip height.
    target = max(8, int(pixel_height * 0.65))
    font = _truetype(target)
    if font is not None:
        return font, 1
    # Pillow default bitmap is ~8px; draw small then NEAREST scale.
    scale = max(1, target // 8)
    return ImageFont.load_default(), scale


def draw_banner_rgb(
    text: str,
    w: int,
    h: int,
    bg: tuple[int, int, int] = (0, 40, 80),
    fg: tuple[int, int, int] = (255, 220, 80),
) -> Image.Image:
    """RGB image with readable left-aligned text, vertically centered."""
    font, scale = fit_font(h)
    if scale == 1:
        img = Image.new("RGB", (w, h), bg)
        draw = ImageDraw.Draw(img)
        bbox = draw.textbbox((0, 0), text, font=font)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        x = 6
        y = max(0, (h - th) // 2 - bbox[1])
        draw.text((x, y), text, fill=fg, font=font)
        return img

    sw, sh = max(1, w // scale), max(1, h // scale)
    small = Image.new("RGB", (sw, sh), bg)
    draw = ImageDraw.Draw(small)
    bbox = draw.textbbox((0, 0), text, font=font)
    th = bbox[3] - bbox[1]
    y = max(0, (sh - th) // 2 - bbox[1])
    draw.text((2, y), text, fill=fg, font=font)
    return small.resize((w, h), Image.Resampling.NEAREST)
