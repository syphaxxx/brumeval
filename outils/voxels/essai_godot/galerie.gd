# Galerie des modèles en voxels : tous les .glb du dossier modeles/ (fabriqués par outils/voxels/voxelise.py),
# rangés en grille sur un pré, sous une lumière de fin d'après-midi et un peu de brume.
# Flèches ou ZQSD : se déplacer ; molette ou A/E : zoomer ; clic droit maintenu : tourner autour.
extends Node3D

const ECART := 3.5  # mètres entre deux modèles
const COLONNES := 16

var camera := Camera3D.new()
var cible := Vector3.ZERO  # point regardé
var distance := 9.0
var angle := 0.0  # rotation autour de la cible, en degrés
var tourne := false


func _ready() -> void:
	var noms := []
	for fichier in DirAccess.get_files_at("res://modeles"):
		if fichier.ends_with(".glb"):
			noms.append(fichier.get_basename())
	noms.sort()
	if noms.is_empty():
		push_warning("Aucun modèle : lancer d'abord python outils/voxels/voxelise.py")
	for n in noms.size():
		var modele: Node3D = load("res://modeles/%s.glb" % noms[n]).instantiate()
		modele.position = Vector3((n % COLONNES) * ECART, 0, (n / COLONNES) * ECART)
		add_child(modele)
		var etiquette := Label3D.new()
		etiquette.text = noms[n]
		etiquette.font_size = 24
		etiquette.position = modele.position + Vector3(0, -0.01, 0.9)
		etiquette.rotation_degrees.x = -90
		add_child(etiquette)
	var lignes := ceili(noms.size() / float(COLONNES))

	var sol := MeshInstance3D.new()
	var plan := PlaneMesh.new()
	plan.size = Vector2(COLONNES * ECART + 10, lignes * ECART + 10)
	sol.mesh = plan
	sol.position = Vector3((COLONNES - 1) * ECART / 2, 0, (lignes - 1) * ECART / 2)
	var herbe := StandardMaterial3D.new()
	herbe.albedo_color = Color(0.36, 0.52, 0.3)
	sol.material_override = herbe
	add_child(sol)

	var soleil := DirectionalLight3D.new()
	soleil.rotation_degrees = Vector3(-45, 35, 0)
	soleil.light_color = Color(1.0, 0.93, 0.82)
	soleil.shadow_enabled = true
	add_child(soleil)

	var monde := WorldEnvironment.new()
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.66, 0.72, 0.8)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.55, 0.6, 0.72)
	env.ambient_light_energy = 0.5
	env.fog_enabled = true  # la brume de Brumeval
	env.fog_light_color = Color(0.72, 0.76, 0.84)
	env.fog_density = 0.012
	monde.environment = env
	add_child(monde)

	add_child(camera)
	placer_camera()


func _process(delta: float) -> void:
	var avance := Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
	# Touches physiques : W A S D d'un clavier QWERTY sont Z Q S D sur un clavier AZERTY (Q/E = A/E en AZERTY).
	if Input.is_physical_key_pressed(KEY_A):
		avance.x -= 1
	if Input.is_physical_key_pressed(KEY_D):
		avance.x += 1
	if Input.is_physical_key_pressed(KEY_W):
		avance.y -= 1
	if Input.is_physical_key_pressed(KEY_S):
		avance.y += 1
	if Input.is_physical_key_pressed(KEY_Q):
		distance = max(3.0, distance - 8 * delta)
	if Input.is_physical_key_pressed(KEY_E):
		distance = min(40.0, distance + 8 * delta)
	if avance != Vector2.ZERO:
		cible += Vector3(avance.x, 0, avance.y).rotated(Vector3.UP, deg_to_rad(angle)) * 10 * delta
	placer_camera()


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			distance = max(3.0, distance * 0.9)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			distance = min(40.0, distance * 1.1)
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			tourne = event.pressed
	elif event is InputEventMouseMotion and tourne:
		angle -= event.relative.x * 0.3


func placer_camera() -> void:
	var recul := Vector3(0, 0.55, 1).normalized() * distance
	camera.position = cible + recul.rotated(Vector3.UP, deg_to_rad(angle))
	camera.look_at(cible + Vector3(0, 1, 0))
