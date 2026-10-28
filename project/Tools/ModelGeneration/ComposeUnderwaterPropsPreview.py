"""Compose the eight Blender renders into a Japanese preview sheet."""
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

kProjectDirectory = Path(__file__).resolve().parents[2]
kAssetDirectory = kProjectDirectory / 'resources/Models/Environment/Ocean/UnderwaterProps'
kFontPath = str(kProjectDirectory / 'resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf')
kTileWidth = 600
kTileHeight = 610
kHeaderHeight = 152
kFooterHeight = 72
kDescriptions = (
    '欠けた門と付着した貝・サンゴ',
    '青緑の結晶で進行方向を誘導',
    '砂に埋もれた顔と弱く光る目',
    '発光する象嵌を備えた回転石輪',
    '巨大生物を予感させる骨のトンネル',
    '鉱物が付着した黒い煙突と気泡',
    '光る先端とゆっくり揺れる葉',
    '折れた柱・倒れた柱とサンゴ',
)


def Font(size):
    return ImageFont.truetype(kFontPath, size)


def Main():
    reports = json.loads((kAssetDirectory / 'asset_info.json').read_text(encoding='utf-8'))
    board = Image.new('RGB', (kTileWidth * 4, kHeaderHeight + kTileHeight * 2 + kFooterHeight), '#0c1820')
    draw = ImageDraw.Draw(board)
    draw.text((32, 18), 'STAGE 01  /  UNDERWATER PROPS', font=Font(24), fill='#58d8c6')
    draw.text((32, 55), '海底オブジェクト  —  8種類の3Dモデル', font=Font(38), fill='#edf4f3')
    draw.text((32, 112), '実際のモデルをBlenderで描画したプレビュー ／ ステージ配置前', font=Font(19), fill='#96b0ba')
    for assetIndex, report in enumerate(reports):
        left = (assetIndex % 4) * kTileWidth
        top = kHeaderHeight + (assetIndex // 4) * kTileHeight
        preview = Image.open(kAssetDirectory / 'previews' / (report['id'] + '.png')).convert('RGB')
        preview = preview.resize((580, 490), Image.Resampling.LANCZOS)
        board.paste(preview, (left + 10, top))
        draw.text((left + 22, top + 497), '%02d  %s' % (assetIndex + 1, report['label']), font=Font(24), fill='#edf4f3')
        draw.text((left + 22, top + 538), kDescriptions[assetIndex], font=Font(19), fill='#a6bdc5')
        draw.text((left + 22, top + 570), '{:,} triangles'.format(report['triangles']), font=Font(16), fill='#58d8c6')
    draw.text((32, board.height - 48), '納品形式: BLEND / GLB / OBJ + MTL + PNG   |   回転装置・海藻はBlender/GLBにアニメーション付き',
              font=Font(20), fill='#a6bdc5')
    board.save(kAssetDirectory / 'previews/underwater_props_contact_sheet.png')
    print(kAssetDirectory / 'previews/underwater_props_contact_sheet.png')


if __name__ == '__main__':
    Main()
