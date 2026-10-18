"""Personnages « chibi » en 3D, rendus en sprites de pixel art (Blender, sans fenêtre).

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b --factory-startup -P outils/blender/personnages.py -- [noms…]
    python outils/blender/pixeliser.py              # puis : réduit les rendus en sprites du jeu (outils/sprites/rendus/)

Chaque personnage de outils/blender/personnages.json est construit avec des formes simples (tête, cheveux, corps,
bras, jambes, chapeau, barbe, tablier) aux couleurs de son sprite de face, puis rendu de face (bas), de dos (haut)
et de profil (droite), en 4 images de marche (0 et 2 au repos, 1 et 3 un pas). Les images, 4 fois plus grandes que
dans l'atlas, vont dans outils/blender/rendus/ ; pixeliser.py les réduit comme les planches (decoupe.py).

Rendu « dessin animé » : chaque matière est une couleur unie avec deux tons (ombre / lumière) selon une lumière qui
vient d'en haut à gauche, et un contour sombre d'un pixel (tracé par pixeliser.py), pour ressembler aux sprites dessinés à la main.
"""
import json
import math
import os
import sys

import bpy

ICI = os.path.dirname(os.path.abspath(__file__))
SORTIE = os.path.join(ICI, 'rendus')
FACTEUR = 4  # rendu 4 fois plus fin que l'atlas
DIRECTIONS = {'bas': 0, 'droite': 90, 'haut': 180}


def couleur(h):
    """« #rrggbb » → couleur linéaire de Blender (les couleurs du sprite sont en sRGB)."""
    h = h.lstrip('#')
    c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return tuple(x / 12.92 if x <= .04045 else ((x + .055) / 1.055) ** 2.4 for x in c) + (1,)


_mats = {}


def matiere(h):
    """Matière « dessin animé » : la couleur, assombrie dans l'ombre (deux tons nets), sans reflet."""
    if h in _mats:
        return _mats[h]
    m = bpy.data.materials.new('m' + h)
    m.use_nodes = True
    n, l = m.node_tree.nodes, m.node_tree.links
    n.clear()
    diff = n.new('ShaderNodeBsdfDiffuse')
    rgb = n.new('ShaderNodeShaderToRGB')
    ramp = n.new('ShaderNodeValToRGB')
    ramp.color_ramp.interpolation = 'CONSTANT'
    ramp.color_ramp.elements[0].color = (.55, .55, .6, 1)  # ombre, un peu bleutée
    ramp.color_ramp.elements[1].position = .12
    ramp.color_ramp.elements[1].color = (1, 1, 1, 1)
    mix = n.new('ShaderNodeMix')
    mix.data_type = 'RGBA'
    mix.blend_type = 'MULTIPLY'
    mix.inputs['Factor'].default_value = 1
    mix.inputs[6].default_value = couleur(h)
    emit = n.new('ShaderNodeEmission')
    out = n.new('ShaderNodeOutputMaterial')
    l.new(diff.outputs[0], rgb.inputs[0])
    l.new(rgb.outputs[0], ramp.inputs[0])
    l.new(ramp.outputs[0], mix.inputs[7])
    l.new(mix.outputs[2], emit.inputs[0])
    l.new(emit.outputs[0], out.inputs[0])
    _mats[h] = m
    return m


def forme(kind, nom, loc, dims, h, parent=None, rot=(0, 0, 0), segments=20):
    """Une forme simple (sphère, cylindre, cube) de taille dims, colorée h, accrochée à parent."""
    if kind == 'sphere':
        bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=segments // 2, radius=1, location=loc)
    elif kind == 'cyl':
        bpy.ops.mesh.primitive_cylinder_add(vertices=segments, radius=1, depth=1, location=loc)
    else:
        bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.object
    o.name = nom
    o.scale = dims
    o.rotation_euler = rot
    if kind != 'cube':
        bpy.ops.object.shade_smooth()
    o.data.materials.append(matiere(h))
    if parent:
        bpy.context.view_layer.update()  # sinon la matrice ne connaît pas encore la taille (cubes)
        mw = o.matrix_world.copy()
        o.parent = parent
        o.matrix_world = mw
    return o


def pivot(nom, loc, parent=None):
    e = bpy.data.objects.new(nom, None)
    bpy.context.scene.collection.objects.link(e)
    e.location = loc
    if parent:
        e.parent = parent
        e.location = (loc[0] - parent.location[0], loc[1] - parent.location[1], loc[2] - parent.location[2])
    return e


def couper(o, loc, dims):
    """Retire d'une forme la partie dans une boîte (frange des cheveux : dégage le visage)."""
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    c = bpy.context.object
    c.scale = dims
    mod = o.modifiers.new('coupe', 'BOOLEAN')
    mod.operation = 'DIFFERENCE'
    mod.object = c
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier='coupe')
    bpy.data.objects.remove(c)


def construire(p):
    """Le personnage p (personnages.json), debout, face à la caméra (−Y). Renvoie la racine et les pivots."""
    k = p.get('taille', 1.0)
    peau, haut, bas = p['peau'], p['haut'], p['bas']
    racine = pivot('racine', (0, 0, 0))
    corps = pivot('corps', (0, 0, 0), racine)
    # Jambes : pivot à la hanche, chaussure au bout
    jambes = []
    for c, x in (('g', -.3), ('d', .3)):
        hanche = pivot('hanche_' + c, (x, 0, 1.1 * k), corps)
        forme('cyl', 'jambe_' + c, (x, 0, .6 * k), (.25, .25, 1.0 * k), bas, hanche)
        forme('cube', 'pied_' + c, (x, -.1, .12), (.36, .55, .24), p.get('chaussures', '#4a3428'), hanche)
        jambes.append(hanche)
    # Corps : le bas (pantalon, salopette) puis le haut (chemise)
    forme('cyl', 'bassin', (0, 0, 1.35 * k), (.6, .42, .55 * k), bas, corps)
    forme('cyl', 'torse', (0, 0, 2.0 * k), (.62, .44, .85 * k), haut, corps)
    if p.get('salopette'):  # bavette devant la chemise
        forme('cube', 'bavette', (0, -.42, 1.95 * k), (.62, .06, .7 * k), bas, corps)
    if p.get('gilet'):
        for x in (-.38, .38):
            forme('cube', 'gilet', (x, -.42, 2.0 * k), (.34, .07, .85 * k), p['gilet'], corps)
    if p.get('jupe'):  # jupe évasée, de la taille aux genoux
        bpy.ops.mesh.primitive_cone_add(vertices=20, radius1=.82, radius2=.6, depth=.95 * k, location=(0, 0, 1.25 * k))
        j = bpy.context.object
        bpy.ops.object.shade_smooth()
        j.data.materials.append(matiere(p['jupe']))
        bpy.context.view_layer.update()
        mw = j.matrix_world.copy()
        j.parent = corps
        j.matrix_world = mw
    if p.get('tablier'):
        forme('cube', 'tablier', (0, -.45, 1.45 * k), (.9, .05, 1.2 * k), p['tablier'], corps)
    if p.get('ceinture'):
        forme('cyl', 'ceinture', (0, 0, 1.62 * k), (.64, .46, .12), p['ceinture'], corps)
    # Bras : pivot à l'épaule, main au bout
    bras = []
    for c, x in (('g', -.8), ('d', .8)):
        epaule = pivot('epaule_' + c, (x, 0, 2.35 * k), corps)
        forme('cyl', 'bras_' + c, (x, 0, 1.85 * k), (.19, .19, .95 * k), p.get('manches', haut), epaule)
        forme('sphere', 'main_' + c, (x, 0, 1.3 * k), (.2, .2, .2), peau, epaule, segments=12)
        bras.append(epaule)
    # Tête : grosse (chibi), yeux sombres, cheveux en casque dont la frange dégage le visage
    tz = 2.45 * k + 1.0
    tete = pivot('tete', (0, 0, tz), corps)
    R = 1.15  # tête de chibi : presque la moitié de la hauteur
    forme('sphere', 'tete', (0, 0, tz + .1), (R, R * .95, R * .97), peau, tete)
    for x in (-.4, .4):
        forme('cube', 'oeil', (x, -R * .93, tz - .05), (.26, .14, .46), '#20141a', tete)
        forme('cube', 'reflet', (x + .04, -R * .95, tz + .04), (.08, .04, .1), '#f4f0e8', tete)
    forme('cube', 'joues', (0, -R * .86, tz - .3), (.9, .05, .1), p.get('joues', '#e89a86'), tete)
    if p.get('barbe'):
        forme('sphere', 'barbe', (0, -.62, tz - .6), (.7, .5, .45), p['barbe'], tete)
    elif p.get('moustache'):
        forme('cube', 'moustache', (0, -1.0, tz - .42), (.46, .1, .12), p['moustache'], tete)
    if p.get('cheveux'):
        ch = forme('sphere', 'cheveux', (0, .08, tz + .2), (R * 1.08, R * 1.06, R * 1.06), p['cheveux'], tete)
        frange = .02 if p.get('coiffure') != 'meche' else -.1
        couper(ch, (0, -1.3, tz - 1.0 + frange), (3, 1.9, 2.0))  # dégage le visage sous la frange
        couper(ch, (0, .2, tz - 1.55), (3, 3, 1.2))              # et le cou
        if p.get('coiffure') == 'longue':  # cheveux longs dans le dos et sur les côtés
            forme('cube', 'cheveux_dos', (0, .55, tz - 1.0), (1.7, .5, 1.6), p['cheveux'], tete)
            for x in (-1.0, 1.0):
                forme('cube', 'meche', (x, .1, tz - .55), (.26, .45, 1.1), p['cheveux'], tete)
    ch = p.get('chapeau')
    if ch:
        t, c = ch['type'], ch['couleur']
        if t == 'paille':
            forme('cyl', 'bord', (0, 0, tz + .75), (1.75, 1.7, .1), c, tete)
            forme('cyl', 'calotte', (0, 0, tz + 1.0), (1.05, 1.02, .55), c, tete)
            if ch.get('ruban'):
                forme('cyl', 'ruban', (0, 0, tz + .84), (1.07, 1.04, .14), ch['ruban'], tete)
        elif t == 'casquette':
            forme('sphere', 'calotte', (0, .05, tz + .4), (1.24, 1.22, .95), c, tete)
            couper(bpy.data.objects['calotte'], (0, 0, tz - .5), (3, 3, 1.6))
            forme('cube', 'visiere', (0, -1.1, tz + .45), (1.1, .7, .1), c, tete, rot=(-.15, 0, 0))
        elif t == 'toque':
            forme('cyl', 'toque', (0, 0, tz + .75), (1.15, 1.12, .8), c, tete)
            forme('sphere', 'toque_haut', (0, 0, tz + 1.15), (1.18, 1.15, .42), c, tete)
    return racine, corps, jambes, bras


def poser(corps, jambes, bras, image):
    """Image de marche : 0 et 2 au repos, 1 et 3 un pas (jambes et bras opposés), le corps se soulève un peu."""
    a = {0: 0, 1: 1, 2: 0, 3: -1}[image]
    jambes[0].rotation_euler = (math.radians(28 * a), 0, 0)
    jambes[1].rotation_euler = (math.radians(-28 * a), 0, 0)
    bras[0].rotation_euler = (math.radians(-24 * a), 0, 0)
    bras[1].rotation_euler = (math.radians(24 * a), 0, 0)
    corps.location.z = .06 * abs(a)


def scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    _mats.clear()  # les matières de la scène précédente n'existent plus
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.film_transparent = True
    sc.render.filter_size = .5
    sc.view_settings.view_transform = 'Standard'
    # Caméra orthographique, légèrement au-dessus (vue des cartes)
    cam = bpy.data.objects.new('camera', bpy.data.cameras.new('camera'))
    sc.collection.objects.link(cam)
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = 8.2
    el = math.radians(14)
    cam.location = (0, -20 * math.cos(el), 3.0 + 20 * math.sin(el))
    cam.rotation_euler = (math.pi / 2 - el, 0, 0)
    sc.camera = cam
    sun = bpy.data.objects.new('soleil', bpy.data.lights.new('soleil', 'SUN'))
    sc.collection.objects.link(sun)
    sun.data.energy = 3
    sun.rotation_euler = (math.radians(50), math.radians(-30), math.radians(-20))  # d'en haut à gauche, devant
    # Contour possible avec Freestyle (désactivé : trop épais une fois réduit)
    sc.render.use_freestyle = False  # le contour est tracé après la réduction (pixeliser.py) : plus net
    sc.render.line_thickness_mode = 'ABSOLUTE'
    sc.render.line_thickness = 3.2
    vl = sc.view_layers[0]
    ls = vl.freestyle_settings.linesets[0] if vl.freestyle_settings.linesets else vl.freestyle_settings.linesets.new('contour')
    ls.select_by_visibility = True
    ls.select_silhouette = ls.select_border = ls.select_crease = True
    if ls.linestyle is None:
        ls.linestyle = bpy.data.linestyles.new('contour')
    ls.linestyle.color = (.07, .045, .04)
    ls.linestyle.thickness = 3.2
    sc.render.resolution_x = 32 * FACTEUR * 2 // 2
    sc.render.resolution_y = 60 * FACTEUR * 2 // 2
    return sc


def main():
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    grand = '--grand' in args  # contrôle : une image en grand de chaque direction, sans contour
    args = [a for a in args if not a.startswith('--')]
    persos = json.load(open(os.path.join(ICI, 'personnages.json'), encoding='utf-8'))
    os.makedirs(SORTIE, exist_ok=True)
    for p in persos:
        if args and p['id'] not in args:
            continue
        sc = scene()
        if grand:
            sc.render.use_freestyle = False
            sc.render.resolution_x, sc.render.resolution_y = 384, 720
        racine, corps, jambes, bras = construire(p)
        for d, angle in DIRECTIONS.items():
            racine.rotation_euler = (0, 0, math.radians(angle))
            for image in range(1 if grand else 4):
                poser(corps, jambes, bras, image)
                sc.render.filepath = os.path.join(SORTIE, f"{p['id']}_{d}_{image}.png")
                bpy.ops.render.render(write_still=True)
        print('rendu :', p['id'])


main()
