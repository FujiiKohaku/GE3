from pathlib import Path
from xml.sax.saxutils import escape
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, PageBreak
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.colors import HexColor
from reportlab.lib.enums import TA_LEFT

kOutputDirectory = Path('C:/Projects/KohakuEngine/output/pdf')
kOutputDirectory.mkdir(parents=True, exist_ok=True)
pdfmetrics.registerFont(TTFont('Japanese', 'C:/Windows/Fonts/msgothic.ttc', subfontIndex=0))
kBodyStyle = ParagraphStyle('Body', fontName='Japanese', fontSize=10, leading=17, wordWrap='CJK', textColor=HexColor('#243447'), spaceAfter=9)
kTitleStyle = ParagraphStyle('Title', parent=kBodyStyle, fontSize=24, leading=33, spaceAfter=16)
kHeadingStyle = ParagraphStyle('Heading', parent=kBodyStyle, fontSize=13, leading=21, textColor=HexColor('#007F86'), spaceBefore=10, spaceAfter=7)
kSmallStyle = ParagraphStyle('Small', parent=kBodyStyle, fontSize=8, leading=12, textColor=HexColor('#526779'))
kEntries = [
{
'title':'01  衝撃で割れ方が変わる破壊',
'name':'Breaking Good: Fracture Modes for Realtime Destruction',
'conference':'SIGGRAPH Asia 2022 / ACM Transactions on Graphics',
'fact':'物体固有の破壊モードを事前に求める研究。実行時の衝撃をモードへ投影し、オンラインの亀裂進展計算を行わずに、衝撃に依存した割れ方を求める。',
'idea':'遺跡の柱を撃つ位置で崩れ方を変える。割れた装甲からボスの弱点が露出する。破壊によって通路が開けば、射撃と進路選択が結び付く。',
'prototype':'石柱1本、弾の着弾位置1点、少数の破片に限定する。まずは着弾位置の違いが破壊結果に現れるかを確認し、その後に破片との衝突を追加する。単純な事前分割モデルへの差し替えは準備用の試作であり、研究手法そのものとは区別する。',
'cost':'実装難度：高。実行負荷：中〜高（推定）。事前計算の時間に加え、破片数・剛体運動・衝突判定・描画が負荷を左右する。論文の高速化は、破壊全体が無負荷という意味ではない。',
'pitch':'「弾の着弾位置を、装甲や地形の破壊結果へ反映する」',
'links':[
('研究・動画・コードの入口','https://www.dgp.toronto.edu/projects/breaking-good/'),
('論文PDF','https://www.dgp.toronto.edu/projects/breaking-good/fracture-harmonics.pdf'),
('著者公開コード','https://github.com/sgsellan/fracture-modes'),
('補足動画','https://www.youtube.com/watch?v=0k_tEk34nJQ')],
'note':'論文PDFは配布リンクを確認。大容量のため今回の閲覧ツールでは本文を取得できず、概要は著者の研究ページに基づく。'},
{
'title':'02  へこみ、伸びて、ちぎれる敵',
'name':'Mesh Mortal Kombat: Real-time voxelized soft-body destruction',
'conference':'SIGGRAPH 2024 Real-Time Live! / Tim McGraw',
'fact':'モデルをボクセル化し、PBD（位置に基づく力学）の制約で形を保つ。隣接ボクセル間の制約を、変形の限界を超えたときに解除して破壊する。計算シェーダーで並列化する方式。',
'idea':'クラゲの傘が着弾でへこみ、集中射撃した触手がちぎれる。柔らかい障害物に穴を開けて通過する。変形を被弾の手応えと弱点露出に使う。',
'prototype':'ボス全身ではなく、触手または柔らかい立方体1個から始める。最初は変形だけ、次に制約の破断、最後に攻撃判定と接続する。',
'cost':'実装難度：非常に高。実行負荷：高（推定）。ボクセル数、制約の反復回数、衝突処理、変形結果の描画が影響する。公開コードはOpenGL系の構成で、現エンジンのDirectX 12へそのまま組み込めるものではない。',
'pitch':'「着弾の力で生物系ボスが変形し、部位が破断する」',
'links':[
('学会公式の概要（同名発表を参照）','https://www.siggraph.org/wp-content/uploads/2024/09/Real-Time-Live.html'),
('発表概要・DOI','https://dl.acm.org/doi/10.1145/3641520.3665307'),
('発表者の公開コード','https://github.com/temcgraw/MeshMortalCombat'),
('詳しい後続論文・MIG 2024','https://web.ics.purdue.edu/~tmcgraw/papers/mcgraw_mig_2024.pdf')],
'note':'発表名は「Kombat」、リポジトリ名は「Combat」。後続論文は Gram-Schmidt voxel constraints for real-time destructible soft bodies。ACM本文の閲覧可否は環境に依存する。'},
{
'title':'03  着弾から侵食が広がる装甲',
'name':'Reaction-Diffusion Textures',
'conference':'SIGGRAPH 1991 / Andrew Witkin, Michael Kass',
'fact':'複数の物質量の反応と拡散を計算し、生物の斑点や縞のような模様を生成する研究。方向や位置によって異なる拡散も扱う。元論文の中心は模様の合成であり、装甲破壊のゲームシステムではない。',
'idea':'着弾地点を種に模様が広がり、一定の状態になった領域の装甲が剥がれる。放置すると回復する。熱量の高い弾ほど侵食が進む設定にすれば、既存の発熱システムと接続できる。',
'prototype':'平面またはUVの分かりやすい装甲1枚で、低解像度の2枚のテクスチャを交互に更新する。着弾UVへ値を書き込み、模様の成長を確認する。ダメージ判定への接続は次段階にする。',
'cost':'実装難度：中〜高。実行負荷：低〜中を目標にできる（推定）。テクスチャ解像度・更新頻度・反復回数・対象数で調整する。UVの継ぎ目と、GPU上の状態をゲーム判定へ渡す方法が課題。研究の模様生成から、破壊・再生へ応用する部分は独自設計になる。',
'pitch':'「弾の熱で表皮の侵食が広がり、弱点が露出するボス」',
'links':[
('研究機関による紹介','https://publications.ri.cmu.edu/reaction-diffusion-textures'),
('論文PDF','https://publications.ri.cmu.edu/storage/publications/pub_files/pub1/witkin_andrew_1991_1/witkin_andrew_1991_1.pdf')],
'note':'古典的な手法なので、技術自体を最新・希少と主張しない。着弾・発熱・再生との組み合わせを作品の特徴として示す。'},
{
'title':'04  低空飛行で砂や雪をえぐる',
'name':'Dynamic Sand Simulation and Rendering in Journey',
'conference':'SIGGRAPH 2012 / John Edwards, thatgamecompany',
'fact':'Journeyの動的な砂丘、足跡や軌跡の物理表現、砂粒の輝きの見せ方を解説する講演。限られた描画能力で、砂の量感をどう表現するかを扱う。',
'idea':'低空飛行で砂が左右に押し出され、ブーストで深い航跡が残る。砂の下に遺跡や敵を隠し、飛行によって露出させる。霧以外の方法で飛行と環境を連動させられる。',
'prototype':'機体周辺の小さな高さマップへ航跡を書き込む。速度と高度で溝の深さ・幅を変える。砂の移動や盛り上がりは後から追加する。この高さマップ案は導入用の提案で、講演手法の忠実な再現を意味しない。',
'cost':'実装難度：中〜高。実行負荷：中（推定）。更新領域・地形の分割数・飛散粒子数に依存する。広いステージでの航跡保持と、見た目の変形を衝突判定へ反映するかを決める必要がある。',
'pitch':'「機体の高度と速度に応じて、砂地へ航跡を刻む」',
'links':[
('講演ページ（Journeyの項目を参照）','https://advances.realtimerendering.com/s2012/index.html'),
('講演スライド・PPTX','https://advances.realtimerendering.com/s2012/thatgamecompany/SandRenderingInJourney_thatgamecompany.pptx'),
('補足映像1・MOV','https://advances.realtimerendering.com/s2012/thatgamecompany/093-Pismo-FluidSand.MOV')],
'note':'スライドと映像は講演ページの配布先を確認。PPTXは今回の閲覧で取得タイムアウト、MOVは閲覧ツールの再生対象外。講演ページには補足映像が計3件ある。'}]

def AddParagraph(story, content, style=kBodyStyle):
    story.append(Paragraph(escape(content), style))

def DrawPage(pageCanvas, document):
    pageCanvas.setStrokeColor(HexColor('#007F86'))
    pageCanvas.line(44, 801, 551, 801)
    pageCanvas.setFont('Japanese', 8)
    pageCanvas.setFillColor(HexColor('#526779'))
    pageCanvas.drawString(44, 816, 'KOHAKU / APPLICATION TECHNOLOGY NOTES')
    pageCanvas.drawString(44, 26, '調査日 2026-11-17  |  研究概要とゲームへの応用提案')
    pageCanvas.drawRightString(551, 26, str(document.page))

story = []
AddParagraph(story, 'レールシューティングの技術資料集', kTitleStyle)
AddParagraph(story, '霧以外の4候補 / 発表資料・実装案・負荷の見通し', kHeadingStyle)
AddParagraph(story, '目的：エンジン機能の列挙ではなく、「撃つ・飛ぶ・狙う」に反応する仕組みを、作品のアピールポイントとして育てる。')
for entry in kEntries:
    AddParagraph(story, entry['title'], kHeadingStyle)
    AddParagraph(story, entry['name'] + ' / ' + entry['conference'], kSmallStyle)
AddParagraph(story, '最初の試作として推奨', kHeadingStyle)
AddParagraph(story, '03の侵食する装甲。既存の発熱射撃と結び付けやすく、装甲1枚から試作できる。物理破壊の迫力を優先するなら01、生物の質感を優先するなら02、飛行の手触りを優先するなら04を選ぶ。これは作品との相性についての提案で、実測評価ではない。')
AddParagraph(story, 'この資料の読み方', kHeadingStyle)
AddParagraph(story, '「研究の要点」は発表元に基づく要約。「ゲームへの応用」「最小試作」「負荷」は本プロジェクト向けの提案・推定。コード変更や性能計測は行っていない。最新発表だけに限定せず、使い方に個性を出せる古典も含めた。')
AddParagraph(story, '各ページの青い資料名はクリック可能。URLをコピーする場合は、同梱の rail_shooter_technology_links.md を使用。原論文や映像は再配布せず、発表元へのリンクを掲載している。', kSmallStyle)
for entry in kEntries:
    story.append(PageBreak())
    AddParagraph(story, entry['title'], kTitleStyle)
    AddParagraph(story, entry['name'], kHeadingStyle)
    AddParagraph(story, entry['conference'], kSmallStyle)
    for label, key in [('研究の要点','fact'),('ゲームへの応用案','idea'),('最小試作','prototype'),('実装難度と実行コストの目安','cost')]:
        AddParagraph(story, label, kHeadingStyle)
        AddParagraph(story, entry[key])
    AddParagraph(story, '作品紹介で伝えるなら', kHeadingStyle)
    AddParagraph(story, entry['pitch'])
    AddParagraph(story, '資料リンク', kHeadingStyle)
    for label, url in entry['links']:
        story.append(Paragraph('<link href="' + escape(url, {'"':'&quot;'}) + '" color="#007F86">' + escape(label) + '</link>', kBodyStyle))
    AddParagraph(story, entry['note'], kSmallStyle)
pdfPath = kOutputDirectory / 'rail_shooter_technology_notes.pdf'
document = SimpleDocTemplate(str(pdfPath), pagesize=(595.28,841.89), rightMargin=44, leftMargin=44, topMargin=60, bottomMargin=48, title='レールシューティング 技術資料集', author='KohakuEngine Research Notes')
document.build(story, onFirstPage=DrawPage, onLaterPages=DrawPage)
markdownLines = ['# レールシューティング 技術資料リンク集', '', '確認日：2026-11-17', '', '研究内容とゲームへの応用案はPDFに整理。以下のURLはコピー可能。原資料は発表元で閲覧。', '']
for entry in kEntries:
    markdownLines.extend(['## ' + entry['title'], '', entry['name'], '', entry['conference'], ''])
    for label, url in entry['links']:
        markdownLines.extend([label, '', url, ''])
    markdownLines.extend(['確認メモ：' + entry['note'], ''])
(kOutputDirectory / 'rail_shooter_technology_links.md').write_text('\n'.join(markdownLines), encoding='utf-8')
print(pdfPath)
