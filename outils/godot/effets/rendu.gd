# Rend les effets des sorts image par image (PNG transparents), avec les particules de Godot.
#
#   godot --path outils/godot/effets --script res://rendu.gd --fixed-fps 16 -- [types…]
#
# Pour chaque type (feu, glace…), une petite scène de particules est jouée dans une fenêtre hors écran ; chaque
# image est copiée en PNG dans outils/godot/effets/rendus/<type>_<k>.png (IMAGES images, LARGEUR x HAUTEUR, deux fois la
# taille dans le jeu). --fixed-fps 16 : le temps avance d'un seizième de seconde par image, quelle que soit la vitesse
# de l'ordinateur ; les graines fixes rendent le résultat identique d'une fois à l'autre.
# Ensuite : python outils/godot/pixeliser_effets.py (palette réduite, rangés dans outils/sprites/rendus/).
extends SceneTree

const LARGEUR := 80
const HAUTEUR := 96
const IMAGES := 8
const TYPES := ["feu", "glace", "foudre", "eau", "plante", "poison", "roche", "vent", "ombre", "esprit", "lumiere",
		"metal", "normal", "soin", "percee", "brisure", "poussiere", "echange"]

var vue: SubViewport
var eclair: Line2D
var hasard := RandomNumberGenerator.new()


func _initialize() -> void:
	root.position = Vector2i(-2000, -2000)  # la fenêtre ne gêne pas (rien n'est lu à l'écran)
	_tout.call_deferred()


func _tout() -> void:
	var demandes := OS.get_cmdline_user_args()
	var sortie := ProjectSettings.globalize_path("res://rendus")
	DirAccess.make_dir_recursive_absolute(sortie)
	for t in TYPES:
		if demandes.size() > 0 and not demandes.has(t):
			continue
		await _rendre(t, sortie)
		print("effet : ", t)
	quit()


func _rendre(t: String, sortie: String) -> void:
	vue = SubViewport.new()
	vue.size = Vector2i(LARGEUR, HAUTEUR)
	vue.transparent_bg = true
	vue.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	vue.canvas_item_default_texture_filter = Viewport.DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST
	root.add_child(vue)
	var centre := Node2D.new()
	centre.position = Vector2(LARGEUR / 2.0, HAUTEUR * 0.62)  # le centre de la cible (le bas laisse la place aux pieds)
	vue.add_child(centre)
	hasard.seed = hash(t)
	eclair = null
	_effet(t, centre)
	for k in IMAGES:
		if eclair:
			_zigzag(k)
		await process_frame
		await RenderingServer.frame_post_draw
		vue.get_texture().get_image().save_png("%s/%s_%d.png" % [sortie, t, k])
	vue.queue_free()
	await process_frame


# Un petit carré blanc (taille n) : la forme de chaque particule, teintée par sa couleur
func _carre(n: int) -> Texture2D:
	var im := Image.create(n, n, false, Image.FORMAT_RGBA8)
	im.fill(Color.WHITE)
	return ImageTexture.create_from_image(im)


func _degrade(couleurs: Array) -> Gradient:
	var g := Gradient.new()
	var o := PackedFloat32Array()
	var c := PackedColorArray()
	for i in couleurs.size():
		o.append(float(i) / max(1, couleurs.size() - 1))
		c.append(Color(couleurs[i]))
	g.offsets = o
	g.colors = c
	return g


# Un disque plein de rayon r (particules rondes : bulles, orbes, éclat central)
func _rond(r: int) -> Texture2D:
	var n := 2 * r + 1
	var im := Image.create(n, n, false, Image.FORMAT_RGBA8)
	for y in n:
		for x in n:
			if (x - r) * (x - r) + (y - r) * (y - r) <= r * r + r * 0.6:
				im.set_pixel(x, y, Color.WHITE)
	return ImageTexture.create_from_image(im)


# Une gerbe de particules ; o : réglages (nombre, duree, vitesse, direction, ecart, gravite, taille, forme, couleurs…)
func _gerbe(parent: Node2D, o: Dictionary) -> CPUParticles2D:
	var p := CPUParticles2D.new()
	p.amount = o.get("nombre", 24)
	p.lifetime = o.get("duree", 0.45)
	p.one_shot = true
	p.explosiveness = o.get("ensemble", 0.8)
	p.use_fixed_seed = true
	p.seed = hasard.randi()
	p.local_coords = true
	p.position = o.get("depart", Vector2.ZERO)
	var zone = o.get("zone", 0.0)
	if zone is Vector2:
		p.emission_shape = CPUParticles2D.EMISSION_SHAPE_RECTANGLE
		p.emission_rect_extents = zone
	elif zone > 0:
		p.emission_shape = CPUParticles2D.EMISSION_SHAPE_SPHERE
		p.emission_sphere_radius = zone
	p.direction = o.get("direction", Vector2.UP)
	p.spread = o.get("ecart", 180.0)
	p.gravity = o.get("gravite", Vector2.ZERO)
	p.initial_velocity_min = o.get("vitesse", Vector2(20, 60)).x
	p.initial_velocity_max = o.get("vitesse", Vector2(20, 60)).y
	p.damping_min = o.get("frein", 0.0)
	p.damping_max = o.get("frein", 0.0)
	p.orbit_velocity_min = o.get("tourne", 0.0)
	p.orbit_velocity_max = o.get("tourne", 0.0)
	p.angular_velocity_min = o.get("rotation", 0.0)
	p.angular_velocity_max = o.get("rotation", 0.0)
	p.scale_amount_min = o.get("taille", Vector2(1, 1)).x
	p.scale_amount_max = o.get("taille", Vector2(1, 1)).y
	if o.has("retrecit"):  # la particule rapetisse jusqu'à disparaître
		var c := Curve.new()
		c.add_point(Vector2(0, 1))
		c.add_point(Vector2(1, 0.15))
		p.scale_amount_curve = c
	var f = o.get("forme", 2)
	p.texture = _rond(-f) if f < 0 else _carre(f)  # forme négative : un disque de ce rayon
	p.color_ramp = _degrade(o.get("couleurs", ["#ffffff", "#ffffff00"]))
	p.emitting = true
	parent.add_child(p)
	return p


func _effet(t: String, c: Node2D) -> void:
	var coeur := {"feu": "#ffd34d", "glace": "#dff4ff", "foudre": "#fff8b0", "eau": "#bfe6ff", "plante": "#d8f8a0",
			"poison": "#e0b0ff", "roche": "#e8dcc8", "vent": "#e8fff4", "ombre": "#8a5ab0", "esprit": "#f0e8ff",
			"lumiere": "#ffffff", "metal": "#ffffff", "normal": "#ffffff", "soin": "#d0ffd8"}
	if coeur.has(t):  # éclat au centre : un disque qui grandit et s'efface aussitôt
		_gerbe(c, {"nombre": 1, "duree": 0.22, "vitesse": Vector2(0, 0), "forme": -7, "taille": Vector2(1, 1),
				"ensemble": 1.0, "couleurs": [coeur[t] + "e0", coeur[t] + "60", coeur[t] + "00"]})
	match t:
		"feu":  # flammes qui montent, braises
			_gerbe(c, {"nombre": 40, "duree": 0.45, "zone": Vector2(14, 4), "depart": Vector2(0, 14), "direction": Vector2.UP,
					"ecart": 18.0, "vitesse": Vector2(70, 130), "gravite": Vector2(0, -90), "taille": Vector2(2, 3.5), "retrecit": true,
					"ensemble": 0.5, "couleurs": ["#fff6b0", "#ffd34d", "#ff8a2a", "#c0301a", "#5a1a1000"]})
			_gerbe(c, {"nombre": 14, "duree": 0.5, "zone": 10.0, "vitesse": Vector2(30, 70), "gravite": Vector2(0, -30),
					"couleurs": ["#ffe066", "#ff7a1a", "#ff3a1a00"]})
		"glace":  # éclats qui jaillissent, givre qui scintille
			_gerbe(c, {"nombre": 22, "duree": 0.5, "vitesse": Vector2(50, 110), "frein": 140.0, "taille": Vector2(1.5, 3),
					"rotation": 200.0, "couleurs": ["#ffffff", "#bfe8ff", "#6fb4ff", "#3a6fd000"]})
			_gerbe(c, {"nombre": 18, "duree": 0.5, "zone": 16.0, "vitesse": Vector2(0, 8), "ensemble": 0.2,
					"couleurs": ["#ffffff", "#dff4ff", "#9fd8ff00"]})
		"foudre":  # un éclair qui tombe en zigzag, des étincelles
			eclair = Line2D.new()
			eclair.width = 3
			eclair.default_color = Color("#fff8b0")
			eclair.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
			c.add_child(eclair)
			_gerbe(c, {"nombre": 26, "duree": 0.35, "depart": Vector2(0, 10), "vitesse": Vector2(50, 120), "frein": 120.0,
					"couleurs": ["#ffffff", "#fff27a", "#ffd34d", "#ffb00000"]})
		"eau":  # gerbe de gouttes qui retombent
			_gerbe(c, {"nombre": 36, "duree": 0.5, "depart": Vector2(0, 8), "direction": Vector2.UP, "ecart": 55.0,
					"vitesse": Vector2(60, 120), "gravite": Vector2(0, 380), "taille": Vector2(1.5, 2.5),
					"couleurs": ["#ffffff", "#bfe6ff", "#7fc8ff", "#3a7fd000"]})
		"plante":  # feuilles qui tourbillonnent
			_gerbe(c, {"nombre": 22, "duree": 0.55, "zone": 18.0, "vitesse": Vector2(5, 20), "tourne": 0.9,
					"rotation": 300.0, "forme": 3, "taille": Vector2(1, 1.5), "ensemble": 0.6,
					"couleurs": ["#d8f8a0", "#a8e070", "#5fbf52", "#2f7a3a00"]})
		"poison":  # bulles violettes qui montent
			_gerbe(c, {"nombre": 26, "duree": 0.55, "zone": Vector2(14, 8), "depart": Vector2(0, 6), "direction": Vector2.UP,
					"ecart": 25.0, "vitesse": Vector2(30, 70), "forme": -2, "taille": Vector2(0.8, 1.6), "ensemble": 0.4,
					"couleurs": ["#f0d0ff", "#c890f0", "#9a50d0", "#5a2a8a00"]})
		"roche":  # pierres qui tombent, poussière
			_gerbe(c, {"nombre": 9, "duree": 0.45, "zone": Vector2(16, 2), "depart": Vector2(0, -40), "direction": Vector2.DOWN,
					"ecart": 8.0, "vitesse": Vector2(60, 90), "gravite": Vector2(0, 420), "forme": 4, "taille": Vector2(1, 1.6),
					"rotation": 150.0, "ensemble": 0.6, "couleurs": ["#c8c0b4", "#8a8278", "#5a504800"]})
			_gerbe(c, {"nombre": 20, "duree": 0.5, "zone": Vector2(16, 2), "depart": Vector2(0, 16), "direction": Vector2.UP,
					"ecart": 70.0, "vitesse": Vector2(10, 35), "taille": Vector2(2, 3), "ensemble": 0.3,
					"couleurs": ["#d8ccb8a0", "#a89880", "#7a6a5800"]})
		"vent":  # tourbillon de traits clairs
			_gerbe(c, {"nombre": 34, "duree": 0.5, "zone": 16.0, "vitesse": Vector2(10, 20), "tourne": 1.6,
					"taille": Vector2(1, 2), "ensemble": 0.3, "couleurs": ["#ffffff", "#d8f8ec", "#a0e0d0", "#70c0b000"]})
		"ombre":  # volutes sombres qui montent
			_gerbe(c, {"nombre": 30, "duree": 0.55, "zone": Vector2(14, 6), "depart": Vector2(0, 8), "direction": Vector2.UP,
					"ecart": 30.0, "vitesse": Vector2(25, 60), "tourne": 0.3, "forme": -3, "taille": Vector2(0.7, 1.4), "ensemble": 0.3,
					"couleurs": ["#8a5ab0", "#4a2a6a", "#2a1a3a", "#10081800"]})
		"esprit":  # orbes qui tournent autour de la cible
			_gerbe(c, {"nombre": 12, "duree": 0.55, "zone": 14.0, "vitesse": Vector2(4, 8), "tourne": 1.2,
					"forme": -2, "taille": Vector2(0.8, 1.2), "ensemble": 1.0, "couleurs": ["#ffffff", "#e8d8ff", "#c8a8ff", "#8a6ad000"]})
		"lumiere":  # rayons qui montent, éclat
			_gerbe(c, {"nombre": 30, "duree": 0.45, "zone": Vector2(10, 2), "depart": Vector2(0, 16), "direction": Vector2.UP,
					"ecart": 6.0, "vitesse": Vector2(80, 140), "taille": Vector2(1, 2), "ensemble": 0.4,
					"couleurs": ["#ffffff", "#fff8d0", "#ffe88a", "#ffd34d00"]})
			_gerbe(c, {"nombre": 12, "duree": 0.4, "vitesse": Vector2(40, 70), "frein": 100.0,
					"couleurs": ["#ffffff", "#fff4c0", "#ffe08000"]})
		"percee":  # Percée (échiquier) : colonne dorée qui jaillit du sol, étincelles qui montent
			_gerbe(c, {"nombre": 40, "duree": 0.5, "zone": Vector2(9, 2), "depart": Vector2(0, 18), "direction": Vector2.UP,
					"ecart": 4.0, "vitesse": Vector2(110, 170), "taille": Vector2(1, 2), "ensemble": 0.5,
					"couleurs": ["#ffffff", "#fff4c0", "#ffd34d", "#ff9a2a00"]})
			_gerbe(c, {"nombre": 18, "duree": 0.5, "zone": Vector2(14, 2), "depart": Vector2(0, 18), "direction": Vector2.UP,
					"ecart": 35.0, "vitesse": Vector2(40, 90), "gravite": Vector2(0, 60), "forme": -1,
					"couleurs": ["#ffffff", "#ffe066", "#ffb02a00"]})
		"brisure":  # Brisure : le bouclier vole en éclats (morceaux bleu acier qui tournent et retombent)
			_gerbe(c, {"nombre": 16, "duree": 0.5, "zone": 6.0, "depart": Vector2(0, -8), "vitesse": Vector2(70, 130),
					"gravite": Vector2(0, 300), "forme": 3, "taille": Vector2(1, 1.8), "rotation": 400.0, "ensemble": 1.0,
					"couleurs": ["#e8f0ff", "#c8d6f0", "#2a4a8a", "#1a2a5a00"]})
			_gerbe(c, {"nombre": 20, "duree": 0.35, "depart": Vector2(0, -8), "vitesse": Vector2(60, 120), "frein": 150.0,
					"ensemble": 1.0, "couleurs": ["#ffffff", "#ffe066", "#ffd34d00"]})
		"poussiere":  # atterrissage d'un bond : nuage de poussière qui s'étale au ras du sol
			_gerbe(c, {"nombre": 22, "duree": 0.5, "zone": Vector2(6, 1), "depart": Vector2(0, 16), "direction": Vector2.UP,
					"ecart": 80.0, "vitesse": Vector2(30, 60), "frein": 90.0, "gravite": Vector2(0, 40), "forme": -2,
					"taille": Vector2(0.6, 1.2), "ensemble": 1.0, "couleurs": ["#f0e6d0", "#c8b898", "#9a8a7000"]})
		"echange":  # échange de cases : deux traînées qui tournent l'une autour de l'autre
			_gerbe(c, {"nombre": 18, "duree": 0.5, "zone": 12.0, "vitesse": Vector2(4, 8), "tourne": 2.0, "forme": -1,
					"ensemble": 1.0, "couleurs": ["#ffffff", "#e0d0ff", "#a080e000"]})
			_gerbe(c, {"nombre": 18, "duree": 0.5, "zone": 6.0, "vitesse": Vector2(4, 8), "tourne": -2.4,
					"ensemble": 1.0, "couleurs": ["#ffffff", "#d0f0ff", "#70b0e000"]})
		"metal":  # étincelles
			_gerbe(c, {"nombre": 30, "duree": 0.35, "vitesse": Vector2(60, 140), "gravite": Vector2(0, 200), "frein": 40.0,
					"couleurs": ["#ffffff", "#e8f0ff", "#a8b8d8", "#6a7a9a00"]})
		"normal":  # impact en étoile
			_gerbe(c, {"nombre": 24, "duree": 0.3, "vitesse": Vector2(80, 130), "frein": 260.0, "taille": Vector2(1.5, 2.5),
					"ensemble": 1.0, "couleurs": ["#ffffff", "#fff0d8", "#ffd0a000"]})
		"soin":  # étincelles vertes qui montent, petites croix
			_gerbe(c, {"nombre": 26, "duree": 0.55, "zone": Vector2(14, 8), "depart": Vector2(0, 10), "direction": Vector2.UP,
					"ecart": 10.0, "vitesse": Vector2(20, 50), "taille": Vector2(1, 2), "ensemble": 0.3,
					"couleurs": ["#ffffff", "#d0ffd8", "#7dffa8", "#3ac06a00"]})
			for i in 4:
				var croix := _gerbe(c, {"nombre": 1, "duree": 0.5, "depart": Vector2(-12 + i * 8, 8 - (i % 2) * 6),
						"direction": Vector2.UP, "ecart": 0.0, "vitesse": Vector2(35, 35), "forme": 5,
						"couleurs": ["#ffffff", "#7dffa8", "#3ac06a00"]})
				croix.texture = _croix()


func _croix() -> Texture2D:
	var im := Image.create(5, 5, false, Image.FORMAT_RGBA8)
	for i in 5:
		im.set_pixel(2, i, Color.WHITE)
		im.set_pixel(i, 2, Color.WHITE)
	return ImageTexture.create_from_image(im)


# Éclair : une ligne brisée du haut jusqu'à la cible, redessinée à chaque image, qui s'efface
func _zigzag(k: int) -> void:
	var pts := PackedVector2Array()
	var y := -HAUTEUR * 0.62
	var x := 0.0
	while y < 8:
		pts.append(Vector2(x, y))
		y += 9
		x = clamp(x + hasard.randf_range(-7, 7), -10, 10)
	pts.append(Vector2(0, 10))
	eclair.points = pts
	eclair.default_color = Color("#fff8b0") if k < 4 else Color(1, 0.95, 0.6, 1.0 - (k - 3) / 5.0)
	eclair.width = 3 if k < 3 else 2
	eclair.visible = k != 1  # un battement
