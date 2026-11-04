"""Trace the user's ink silhouettes, assigning ink to hand-reviewed pen routes.

Authoring dependencies (in .tools): Pillow, numpy, vtracer.
No raster data or authoring dependencies are used by the CGI runtime.
"""
from pathlib import Path
import io
import math
import xml.etree.ElementTree as E
import numpy as np
from PIL import Image, ImageFilter, ImageDraw
import vtracer
import resvg_py

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'lettering'
URI = 'http://www.w3.org/2000/svg'
E.register_namespace('', URI)
def tag(s): return '{' + URI + '}' + s

# Routes follow the supplied cursive gestures. Separate characters never share
# a route; no synthetic connections are added between words or characters.
NI = [
 [(154,20),(152,39),(141,62),(120,87),(95,111),(68,132),(48,140)],
 [(112,98),(109,132),(105,177),(101,221),(100,254)],
 [(208,18),(209,36),(199,58),(180,80),(159,100)],
 [(191,87),(211,82),(226,82),(232,87),(221,103),(203,120),(188,131)],
 # The small cursive Z is drawn before the vertical/hook, followed by its bar.
 [(191,139),(198,135),(201,144),(189,155),(176,160),(195,147),(199,148)],
 [(199,148),(202,164),(203,190),(208,222),(211,248),(207,256),(196,252),
  (177,235),(158,217),(152,206)],
 [(152,206),(157,199),(178,191),(205,183),(237,176),(259,173)],
 [(392,80),(395,92),(391,114),(383,139),(376,162),(377,176),
  (391,194),(411,213),(430,235),(440,254),(438,265)],
 [(419,157),(415,179),(402,204),(385,228),(366,250),(353,264)],
 [(349,148),(369,141),(395,137),(422,131)],
 [(496,54),(504,69),(506,91),(505,119),(507,150),(513,186),
  (523,222),(528,245),(522,253),(511,248),(487,222),(465,201)],
 [(425,183),(449,170),(479,158),(511,148),(547,141),(578,140),(597,141)]
]
WO = [
 [(122,25),(126,39),(119,59),(103,83),(80,111),(56,142),(36,172),
  (40,176),(63,171),(88,164),(113,160),(133,159)],
 [(82,135),(88,158),(92,189),(95,218),(94,241),(84,261),(69,266),
  (52,263),(43,256),(44,249)],
 # The rising ti is a separate pass, tapering into the following diagonal.
 [(44,249),(62,236),(87,222),(114,206),(139,186),(152,170)],
 # One continuous diagonal-hook: down with pressure, then up lightly to meet
 # the final pie. There is NO later dot/retracing stroke to thicken it again.
 [(132,96),(145,132),(162,171),(181,211),(204,247),(228,278),
  (242,295),(244,295),(242,280),(229,254),(212,225),(194,204)],
 [(205,140),(210,151),(208,169),(200,190),(185,218),(168,248),(145,289)],
 [(291,87),(297,111),(305,135),(312,147)],
 [(319,63),(339,59),(363,60),(378,71),(381,89),(379,114),(375,139),(367,164)],
 [(319,110),(336,111),(349,114),(340,124),(326,135),(314,145)],
 [(314,149),(334,144),(354,145),(359,148),(346,160),(326,177),(308,192),
  (310,195),(331,188),(351,182),(361,186),(353,197),(346,210),(344,230),(345,254)],
 [(355,237),(367,232),(378,231)],
 [(291,227),(290,244),(282,261),(276,272),(283,270),(294,257),(297,250)],
 [(288,252),(316,256),(348,266),(385,278),(426,288),(466,299),(502,304),(526,300)]
]
COMMA = [[(614,256),(615,267),(609,286),(597,310),(580,335)]]

# One smooth pressure outline for 我's diagonal-hook. The outer descending body
# and the light ascending return form one continuous stroke, not polygon seams.
WO_DIAGONAL = ('M132,91 C137,90 139,105 144,118 '
    'C161,163 181,205 205,245 C215,262 225,276 233,282 '
    'C230,265 213,231 194,207 C190,202 190,200 192,200 '
    'C210,214 234,249 245,276 C249,285 253,296 248,300 '
    'C244,305 233,295 222,283 C201,259 184,236 170,211 '
    'C153,181 139,148 129,113 C124,102 124,94 132,91 Z')
WO_PIE = ('M198,136 C204,129 213,131 216,141 '
    'C220,155 210,184 200,205 C187,234 164,273 147,296 '
    'C144,300 140,303 137,302 C144,284 157,260 172,235 '
    'C188,207 200,177 203,153 C204,147 202,145 198,147 '
    'C193,150 192,147 195,142 C196,139 197,137 198,136 Z')

# Each 是 gesture has its own smooth ink outline. Sharing a traced silhouette
# partition allowed pieces of the later downward stroke to appear inside 日.
SHI_Z_LEGS = (
    'M313,144 C326,143 343,139 356,141 C363,141 369,144 364,150 '
    'C357,156 334,153 315,154 C310,154 307,151 309,147 Z',
    'M357,147 Q362,146 364,151 C350,165 326,185 309,198 '
    'Q303,201 302,196 C303,192 307,188 313,183 C329,169 345,155 357,147 Z',
    'M306,190 C324,186 343,177 359,178 C368,179 370,183 363,190 '
    'C350,196 326,198 310,200 Q302,202 303,196 Q302,192 306,190 Z',
    'M358,184 C365,183 369,186 363,192 C352,202 349,215 350,234 '
    'L350,249 Q350,257 343,255 Q338,255 339,247 L339,221 '
    'C338,209 343,197 352,189 Q353,187 358,184 Z',
)
EXACT_WO = {
    # Continuous rising ti: overlap the shaft at its start and reach inside
    # the following diagonal. The old exclusive partition clipped off its tip.
    2: ('M43,245 C61,232 85,218 109,203 C130,188 146,176 154,163 '
        'C156,161 160,163 159,167 C151,185 131,202 113,213 '
        'C89,229 65,240 47,253 C43,256 39,250 43,245 Z'),
    3: WO_DIAGONAL,
    4: WO_PIE,
    5: ('M288,84 C290,80 294,83 296,89 L310,132 '
        'Q312,140 314,144 Q316,147 312,151 Q307,154 303,147 '
        'C298,137 291,111 287,94 Q285,88 288,84 Z'),
    6: ('M318,55 C335,47 360,48 373,55 C388,63 392,75 390,93 '
        'C388,123 380,151 372,166 C369,172 364,172 362,167 '
        'C367,144 371,121 372,98 C374,80 371,73 360,70 '
        'C346,68 332,70 319,68 C310,67 307,62 312,58 Z'),
    7: ('M318,105 C328,103 343,106 352,109 Q360,112 354,118 '
        'C343,128 328,138 316,145 Q312,147 310,144 Q310,140 315,136 '
        'L340,117 C331,116 323,116 319,116 Q312,115 313,110 Q314,107 318,105 Z'),
    8: ' '.join(SHI_Z_LEGS),
    9: ('M354,233 C361,229 373,225 381,226 Q390,229 381,234 '
        'C371,238 363,240 355,240 Q350,239 351,237 Z'),
    10: ('M287,224 C290,217 296,220 297,228 C299,243 291,261 279,276 '
         'Q273,280 270,275 C270,267 277,256 281,246 C285,237 282,229 287,224 Z'),
    11: ('M287,244 C310,244 342,254 374,263 C420,278 468,292 505,295 '
         'Q519,296 531,294 Q535,297 528,303 C509,315 477,311 448,307 '
         'C403,300 360,283 326,271 L290,259 C283,256 282,249 287,244 Z'),
}


def traced(binary):
    # Smooth original outline, rather than synthesizing a generic brush font.
    im = Image.fromarray(np.where(binary, 0, 255).astype('uint8'), 'L')
    stream = io.BytesIO(); im.save(stream, format='PNG')
    svg = vtracer.convert_raw_image_to_svg(stream.getvalue(), img_format='png',
        colormode='binary', mode='spline', filter_speckle=2,
        corner_threshold=60, length_threshold=1.5, splice_threshold=30,
        path_precision=3)
    root = E.fromstring(svg)
    return [p for p in root.iter(tag('path'))]


def make(name, image_path, crop, routes, remove_background=False):
    original = Image.open(image_path).convert('L')
    left, top, right, bottom = crop
    grey = np.asarray(original.crop(crop).filter(ImageFilter.GaussianBlur(.35)))
    ink = grey < 170
    yy, xx = np.indices(ink.shape)
    if name == 'ni':
        ink &= ~((xx + left > 560) & (yy + top > 225))
    if remove_background:
        # Exclude the app canvas below the oblique white-paper edge.
        ink &= (yy + top < 270 + (xx + left) * .245)
    diagonal_shape=None
    pie_shape=None
    exact_pixels={}
    if name == 'wo':
        def outline_pixels(d):
            markup=(f'<svg xmlns="{URI}" width="{right-left}" height="{bottom-top}">'
                    f'<path transform="translate({-left} {-top})" d="{d}"/></svg>')
            rgba=Image.open(io.BytesIO(resvg_py.svg_to_bytes(svg_string=markup))).convert('RGBA')
            return np.asarray(rgba)[:,:,3]>127
        exact_pixels={index:outline_pixels(d) for index,d in EXACT_WO.items()}
        diagonal_shape=exact_pixels[3]
        pie_shape=exact_pixels[4]
        # Preserve the original final pie and the end of the earlier ti. Only
        # the diagonal's rough boundary is replaced by the smooth outline.
        keep=Image.new('L',ink.shape[::-1],0);pen=ImageDraw.Draw(keep)
        for route,width in [(WO[4],28),(WO[2],14)]:
            pen.line([(x-left,y-top) for x,y in route],fill=255,width=width,joint='curve')
        keep=np.asarray(keep)>0
        zone=(xx+left>120)&(xx+left<260)&(yy+top>85)&(yy+top<310)
        ink=np.where(zone,diagonal_shape | pie_shape | (ink & keep),ink)
        ink |= exact_pixels[2]
        shi_zone=(xx+left>265)&(yy+top>30)
        shi_ink=np.logical_or.reduce([exact_pixels[i] for i in range(5,12)])
        ink=np.where(shi_zone,shi_ink,ink)
    routes = [[(x-left,y-top) for x,y in route] for route in routes]
    ys, xs = np.nonzero(ink)
    owners = np.zeros(len(xs), dtype='int16')
    best = np.full(len(xs), np.inf)
    distances = []
    progressions = []
    for index, route in enumerate(routes):
        distance = np.full(len(xs), np.inf)
        progression = np.zeros(len(xs))
        first_core = np.full(len(xs), np.inf)
        travelled = 0.0
        for (ax,ay),(bx,by) in zip(route,route[1:]):
            dx,dy=bx-ax,by-ay
            u=np.clip(((xs-ax)*dx+(ys-ay)*dy)/(dx*dx+dy*dy or 1),0,1)
            segment_distance=(xs-ax-u*dx)**2+(ys-ay-u*dy)**2
            segment_length=math.hypot(dx,dy)
            along=travelled+u*segment_length
            nearer=segment_distance<distance
            progression[nearer]=along[nearer]
            distance=np.minimum(distance,segment_distance)
            first_core=np.minimum(first_core,np.where(segment_distance<=16,along,np.inf))
            travelled+=segment_length
        # A self-crossing route paints its central ink on the first pass.
        if not (name == 'wo' and index == 3):
            progression=np.where(np.isfinite(first_core),first_core,progression)
        distances.append(distance)
        progressions.append(progression)
        take=distance<best
        owners[take]=index;best[take]=distance[take]
    if name == 'ni':
        z, stem, bar, woman, son, son_bar = 4, 5, 6, 7, 10, 11
        gx, gy = xs + left, ys + top
        # Preserve the little Z as its own leading movement. Only its small
        # overlap with the stem is shared; the stem does not appear in advance.
        z_core = (gx > 173) & (gx < 205) & (gy > 130) & (gy < 163)
        owners[z_core & (distances[z] < 16)] = z
        # Keep a smooth vertical through the broad crossing in 你's right side.
        # The transverse wings belong to the final bar, not the vertical/hook.
        crossing = (gx > 170) & (gx < 239) & (gy >= 160) & (gy <= 212)
        stem_left = 194 + (gy - 160) * .06
        stem_right = 209 + (gy - 160) * .15
        shaft = (gx >= stem_left) & (gx <= stem_right)
        owners[crossing & shaft] = stem
        owners[crossing & ~shaft & ((owners == stem) | (owners == bar))] = bar
        # The first 女 movement must remain unbroken through both crossings.
        # Reserve its central ink corridor before later horizontal/pie strokes.
        woman_crossings = (((gy >= 125) & (gy <= 161)) |
                           ((gy >= 181) & (gy <= 224))) & (gx > 365) & (gx < 431)
        owners[woman_crossings & (distances[woman] <= 36)] = woman
        # At the 子 crossing, nearest-route assignment produces triangular
        # bar fragments on the vertical. Keep a clean shaft through the
        # junction, and reserve its lateral wings for the final horizontal.
        crossing = (gx > 488) & (gx < 537) & (gy > 124) & (gy < 176)
        shaft_left = 500 + (gy - 130) * .09
        shaft_right = 510 + (gy - 130) * .14
        shaft = (gx >= shaft_left) & (gx <= shaft_right)
        owners[crossing & shaft] = son
        owners[crossing & ~shaft & ((owners == son) | (owners == son_bar))] = son_bar
        # The beginning of the same horizontal brushes past 女's last downstroke.
        # This isolated tip also belongs to the horizontal, not the 女 stroke.
        owners[(gx >= 417) & (gx < 435) & (gy >= 173) & (gy < 195)] = son_bar
    if name == 'wo':
        gx, gy = xs + left, ys + top
        # Keep the first continuous pie-to-horizontal gesture whole where the
        # later vertical crosses it; do not attach its sideways ends to the竖.
        horizontal=(gx>64)&(gx<119)&(gy>149)&(gy<181)
        owners[horizontal & (distances[0]<=16)] = 0
        # Deliberately outline the shaft through its later ti crossing. A
        # nearest-path partition otherwise adds lateral triangular bumps.
        junction=(gx>40)&(gx<123)&(gy>=181)&(gy<=243)
        shaft_left=np.interp(gy,[181,190,210,233,238,243],[86,87,88,88,86,82])
        shaft_right=np.interp(gy,[181,190,210,233,238,243],[97,98,100,100,99,98])
        vertical_core=(gx>=shaft_left)&(gx<=shaft_right)
        owners[junction & vertical_core] = 1
        owners[junction & ~vertical_core & ((owners==1)|(owners==2))] = 2
        # The ti finishes at the edge of the diagonal, with no detached cap.
        owners[(gx>142)&(gx<172)&(gy<179)&(gy>145)&(owners==2)] = 3
        # Explicit smooth left/right profiles separate the diagonal's pressure
        # body from the final pie at their crossing. Nearest-distance regions
        # otherwise make the diagonal suddenly bulge and then thin again.
        profile_y=[165,175,185,195,205,215,225,235,245,255]
        body_left=np.interp(gy,profile_y,[150,154,158,163,168,174,180,185,191,198])
        body_right=np.interp(gy,profile_y,[165,170,175,181,187,194,200,207,214,222])
        junction=(gx>141)&(gx<235)&(gy>=165)&(gy<=255)
        body=(gx>=body_left)&(gx<=body_right)
        owners[junction & body] = 3
        wings=junction & ~body & (distances[4]<225)
        owners[wings & (owners==3)] = 4
        # The slender return hook is already part of the preceding gesture.
        # Assign it BEFORE the pie and taper it toward their meeting point.
        returning=np.full(len(xs),np.inf)
        up_route=routes[3][6:]
        for (ax,ay),(bx,by) in zip(up_route,up_route[1:]):
            dx,dy=bx-ax,by-ay
            u=np.clip(((xs-ax)*dx+(ys-ay)*dy)/(dx*dx+dy*dy or 1),0,1)
            returning=np.minimum(returning,(xs-ax-u*dx)**2+(ys-ay-u*dy)**2)
        taper=np.interp(gy,[204,218,245,275,291],[1.3,2.4,3.2,6,10])
        owners[(returning<=taper*taper)&(gx>=193)&(gy>=204)&(gy<=291)] = 3
        owners[diagonal_shape[ys,xs]] = 3
        # The downstroke and ascending return use separate progress domains.
        # A first/nearest-core rule lights the return's tip before the bottom
        # turn is reached, making a spurious little hook appear mid-gesture.
        down=routes[3][:8];up=routes[3][7:]
        def project_progress(points):
            dist=np.full(len(xs),np.inf);position=np.zeros(len(xs));total=0.0
            for (ax,ay),(bx,by) in zip(points,points[1:]):
                dx,dy=bx-ax,by-ay;length=math.hypot(dx,dy)
                u=np.clip(((xs-ax)*dx+(ys-ay)*dy)/(dx*dx+dy*dy or 1),0,1)
                d=(xs-ax-u*dx)**2+(ys-ay-u*dy)**2
                nearer=d<dist;position[nearer]=(total+u*length)[nearer]
                dist=np.minimum(dist,d);total+=length
            return position,total
        down_position,down_length=project_progress(down)
        up_position,_=project_progress(up)
        divider=np.interp(gy,[198,200,220,240,260,280,286],
                          [187,188,200,211,222,234,242])
        ascending=(gx>divider)&(gy>=198)&(gy<286)
        turn=gy>=284
        down_position[turn]=np.interp(gy[turn],[284,301],[down_length-13,down_length])
        progressions[3]=np.where(ascending,down_length+up_position,down_position)
    # Region overrides may give ink to a route other than its nearest route.
    # Size that route's reveal mask against its OWN distances, avoiding cutouts.
    best = np.stack(distances)[owners, np.arange(len(owners))]
    along = np.stack(progressions)[owners, np.arange(len(owners))]
    width,height=right-left,bottom-top
    root=E.Element(tag('svg'),{'viewBox':f'0 0 {width} {height}',
        'width':str(width),'height':str(height),'role':'img','aria-label':name})
    E.SubElement(root,tag('metadata')).text=(
        'Traced from user-supplied handwriting. Outline gently smoothed. '
        'Pen routes reviewed manually; connections stay within each character.')
    defs=E.SubElement(root,tag('defs'))
    clip=E.SubElement(defs,tag('clipPath'),{'id':'ink-outline','clipPathUnits':'userSpaceOnUse'})
    for p in traced(ink): clip.append(p)
    group=E.SubElement(root,tag('g'),{'fill':'#000'})
    lengths=[sum(math.dist(a,b) for a,b in zip(r,r[1:])) for r in routes]
    time=.35
    root.set('data-timeline-start',str(time))
    # Long movements get more time; maintain a readable minimum for short marks.
    durations=[max(.12,length/450) for length in lengths]
    for i,(route,duration) in enumerate(zip(routes,durations)):
        part=np.zeros(ink.shape,dtype='uint8');selected=owners==i
        if name == 'wo' and i in EXACT_WO:
            # Independent full outlines overlap naturally at crossings. No
            # later gesture's ink is borrowed by an earlier stroke.
            selected=exact_pixels[i][ys,xs]
        part[ys[selected],xs[selected]]=255
        # One-pixel overlap prevents tracing seams inside the overall outline.
        expanded=np.asarray(Image.fromarray(part).filter(ImageFilter.MaxFilter(3)))>0
        if name == 'wo' and i in (1, 3):
            # Preserve the authored shaft/diagonal edges at crossings. Expanding
            # these regions pulls later-stroke ink into small outward bumps.
            expanded=part>0
        shapes=traced(expanded & ink)
        if name == 'wo' and i in EXACT_WO:
            exact_outline=EXACT_WO[i]
            shapes=[E.Element(tag('path'),{'d':exact_outline,
                'transform':f'translate({-left} {-top})'})]
        mask=E.SubElement(defs,tag('mask'),{'id':f'm{i}','maskUnits':'userSpaceOnUse',
            'x':'-100','y':'-100','width':str(width+200),'height':str(height+200)})
        continuous_turn = name == 'wo' and i == 3
        phased_shi = name == 'wo' and i == 8
        if continuous_turn:
            # A continuous geometric front avoids the temporary extra spur
            # produced by quantized ink bands at the bottom reversal.
            domain=E.SubElement(defs,tag('clipPath'),{'id':'wo-down-domain',
                'clipPathUnits':'userSpaceOnUse'})
            E.SubElement(domain,tag('path'),{'transform':f'translate({-left} {-top})',
                'd':('M-100,-100 L500,-100 L500,118 L144,118 '
                     'C161,163 181,205 205,245 C215,262 225,276 233,282 '
                     'L250,294 '
                     'L500,294 L500,500 L-100,500 Z')})
            down=E.SubElement(mask,tag('g'),{'clip-path':'url(#wo-down-domain)'})
            front=E.SubElement(down,tag('rect'),{'x':'-100','y':str(91-top),
                'width':'1000','height':'0','fill':'#fff'})
            down_duration=duration*.7
            E.SubElement(front,tag('animate'),{'attributeName':'height','from':'0','to':'210',
                'begin':f'{time:.7f}s','dur':f'{down_duration:.7f}s','fill':'freeze'})
            up=E.SubElement(mask,tag('rect'),{'x':'-100','y':str(301-top),
                'width':'1000','height':'0','fill':'#fff'})
            for attr,a,b in [('y',301-top,200-top),('height',0,101)]:
                E.SubElement(up,tag('animate'),{'attributeName':attr,'from':str(a),'to':str(b),
                    'begin':f'{time+down_duration:.7f}s','dur':f'{duration-down_duration:.7f}s',
                    'fill':'freeze'})
        if phased_shi:
            # 日's bottom Z is a single cursive gesture with four ordered legs.
            # Restrict each leg geometrically so a nearby return/descender is
            # never uncovered by the first horizontal or descending diagonal.
            phases=[
                (.20, SHI_Z_LEGS[0], 'x', 307-left, 371-left, 138-top, 19),
                (.28, SHI_Z_LEGS[1], 'y', 146-top, 202-top, 297-left, 76),
                (.22, SHI_Z_LEGS[2], 'x', 301-left, 371-left, 175-top, 29),
                (.30, SHI_Z_LEGS[3], 'y', 182-top, 258-top, 336-left, 38),
            ]
            begin=time
            for n,(fraction,domain_d,axis,a,b,fixed,extent) in enumerate(phases):
                holder=mask
                if domain_d:
                    domain_id=f'shi-leg-{n}'
                    clip=E.SubElement(defs,tag('clipPath'),{'id':domain_id,
                        'clipPathUnits':'userSpaceOnUse'})
                    E.SubElement(clip,tag('path'),{'d':domain_d,
                        'transform':f'translate({-left} {-top})'})
                    holder=E.SubElement(mask,tag('g'),{'clip-path':f'url(#{domain_id})'})
                attrs=({'x':str(a),'y':str(fixed),'width':'0','height':str(extent)}
                       if axis=='x' else
                       {'x':str(fixed),'y':str(a),'width':str(extent),'height':'0'})
                attrs['fill']='#fff'
                rect=E.SubElement(holder,tag('rect'),attrs)
                E.SubElement(rect,tag('animate'),{'attributeName':'width' if axis=='x' else 'height',
                    'from':'0','to':str(b-a),'begin':f'{begin:.7f}s',
                    'dur':f'{duration*fraction:.7f}s','fill':'freeze'})
                begin+=duration*fraction
        # Reveal a narrow band of actual ink at each pen-progress step. A wide
        # moving circular dash mask can uncover two distant sections at once
        # when a cursive route turns back near itself. These local vector bands
        # are ordered by distance along the pen route instead.
        count=0 if continuous_turn or phased_shi else max(8,math.ceil(lengths[i]/7.5))
        bins=np.minimum(count-1,(progressions[i]/lengths[i]*count).astype('int32'))
        for j in range(count):
            active=selected & (bins==j)
            if not active.any():continue
            band=np.zeros(ink.shape,dtype='uint8');band[ys[active],xs[active]]=255
            band=np.asarray(Image.fromarray(band).filter(ImageFilter.MaxFilter(5)))>0
            pieces=traced(band & expanded & ink)
            if not pieces:continue
            reveal=E.SubElement(mask,tag('g'),{'fill':'#fff','visibility':'hidden'})
            E.SubElement(reveal,tag('set'),{'attributeName':'visibility','to':'visible',
                'begin':f'{time+duration*j/count:.7f}s','fill':'freeze'})
            for p in pieces:
                p.set('fill','#fff');reveal.append(p)
        # Explicit timing metadata includes the final band's completion time.
        mask.set('data-begin',str(time));mask.set('data-duration',str(duration))
        if name == 'wo' and i in EXACT_WO:
            # Complete with the exact smooth vector edge rather than retaining
            # subpixel seams from the individual reveal bands.
            edge=E.SubElement(mask,tag('path'),{'d':exact_outline,
                'transform':f'translate({-left} {-top})','fill':'#fff','visibility':'hidden'})
            E.SubElement(edge,tag('set'),{'attributeName':'visibility','to':'visible',
                'begin':f'{time+duration-.000001:.7f}s','fill':'freeze'})
        attributes={'mask':f'url(#m{i})'}
        if not (name == 'wo' and i in EXACT_WO):
            attributes['clip-path']='url(#ink-outline)'
        g=E.SubElement(group,tag('g'),attributes)
        for p in shapes:g.append(p)
        time+=duration
        if (name == 'ni' and i == 6) or (name == 'wo' and i == 4):
            time += .07  # Explicit pen lift between the two Chinese characters.
    E.indent(root)
    root.set('data-timeline-end',str(time))
    E.ElementTree(root).write(OUT/f'{name}.svg',encoding='utf-8',xml_declaration=True)
    Image.fromarray(np.where(ink,0,255).astype('uint8')).save(OUT/f'{name}-trace-target.png')
    print(name,len(routes),'pen routes;',len(xs),'ink pixels;',f'{time:.3f}s source timeline')


make('ni',OUT/'source/ni-comma.png',(30,0,620,280),NI)
make('comma',OUT/'source/ni-comma.png',(570,240,635,350),COMMA)
make('wo',OUT/'source/wo-shi.jpg',(25,10,540,325),WO,True)
