"""Compose the rendered distant landmarks into a Japanese review sheet."""
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

kProjectDirectory = Path(__file__).resolve().parents[2]
kAssetDirectory = kProjectDirectory / 'resources/Models/Environment/Ocean/DistantRuins'
kPreviewDirectory = kAssetDirectory / 'previews'
kFontPath = str(kProjectDirectory / 'resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf')


def Font(size):
    return ImageFont.truetype(kFontPath, size)


def Main():
    report = json.loads((kAssetDirectory / 'asset_info.json').read_text(encoding='utf-8'))
    board = Image.new('RGB', (1680, 1710), '#0b1b24')
    draw = ImageDraw.Draw(board)
    draw.text((28, 14), 'STAGE 01 / DISTANT UNDERWATER LANDMARKS', font=Font(21), fill='#5ed6c9')
    draw.text((28, 49), '霧の奥に見える、海底遺跡', font=Font(34), fill='#e7f2f2')
    hero = Image.open(kPreviewDirectory / 'distant_ruins_atmosphere.png').convert('RGB')
    board.paste(hero.resize((1680, 1000), Image.Resampling.LANCZOS), (0, 110))
    for assetIndex, asset in enumerate(report['assets']):
        left = assetIndex * 560
        preview = Image.open(kPreviewDirectory / (asset['id'] + '.png')).convert('RGB')
        board.paste(preview.resize((540, 432), Image.Resampling.LANCZOS), (left + 10, 1125))
        draw.text((left + 22, 1569), asset['label'], font=Font(25), fill='#e7f2f2')
        draw.text((left + 22, 1611), '{:,} triangles'.format(asset['triangles']), font=Font(18), fill='#5ed6c9')
    draw.text((28, 1668), '実際の3DモデルをBlenderで描画 ／ ゲームへの配置は未変更 ／ BLEND・GLB・OBJを保存済み',
              font=Font(18), fill='#a0b7c1')
    board.save(kPreviewDirectory / 'distant_ruins_preview_sheet.png')
    print(kPreviewDirectory / 'distant_ruins_preview_sheet.png')


if __name__ == '__main__':
    Main()
