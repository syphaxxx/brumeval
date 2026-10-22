"""Le mot du Bestiaire (étape « 20 heures », 2026-10-20) : une courte description pour chaque créature, montrée dans la
fiche du Bestiaire une fois l'espèce obtenue (champ « description » de data/especes.json), et sa traduction anglaise
(data/langues/en.json, à appliquer après `python outils/textes.py`). Une seule fois (refuse si une description existe).

    python outils/villes/descriptions.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

D = carte.R
DESC = {
    'braisenard': ("Sa queue de flammes s'allume quand il est content. Il dort roulé autour.", "Its flaming tail lights up when it's happy. It sleeps curled around it."),
    'gouttelin': ("Une goutte vivante qui adore la pluie. Il rebondit pour dire bonjour.", "A living droplet that loves the rain. It bounces to say hello."),
    'ronceau': ("Un bourgeon têtu. Ses épines repoussent dès qu'on les coupe.", "A stubborn bud. Its thorns grow back as soon as they're cut."),
    'piafouine': ("Elle siffle au lever du soleil, toujours la même chanson.", "It whistles at sunrise, always the same tune."),
    'mulotin': ("Il cache des graines partout et oublie toujours où.", "It hides seeds everywhere and always forgets where."),
    'lucioline': ("Sa lumière guide les voyageurs perdus… ou les égare, selon son humeur.", "Its light guides lost travelers… or misleads them, depending on its mood."),
    'grenouillon': ("Il chante sous la pluie, très faux, mais avec beaucoup de cœur.", "It sings in the rain, badly out of tune, but with a lot of heart."),
    'champichou': ("Ses spores font éternuer les créatures qui passent trop près.", "Its spores make creatures that pass too close sneeze."),
    'rocaillou': ("On le prend souvent pour un caillou. Il en profite pour faire la sieste.", "It's often mistaken for a pebble. It uses that to take naps."),
    'brumelin': ("Un petit nuage de brume qui suit les voyageurs en silence.", "A small cloud of mist that follows travelers in silence."),
    'tisonnel': ("Il lèche les braises comme d'autres lèchent du miel.", "It licks embers the way others lick honey."),
    'voltigeon': ("Ses plumes crépitent avant l'orage. Les bergers s'en servent comme baromètre.", "Its feathers crackle before a storm. Shepherds use it as a barometer."),
    'galetor': ("Plus il roule, plus il est lisse, plus il est fier.", "The more it rolls, the smoother it gets, and the prouder it is."),
    'glaconnet': ("Il fond un peu quand on le serre dans ses bras. Il se laisse faire quand même.", "It melts a little when hugged. It lets you anyway."),
    'fumerole': ("Elle sort des fissures du volcan, chaude et un peu toxique.", "It rises from the volcano's cracks, warm and a little toxic."),
    'serpentin': ("Il ondule dans les herbes hautes sans un bruit.", "It slithers through tall grass without a sound."),
    'chauvenuit': ("Elle ne sort qu'à la nuit tombée et dort la tête en bas.", "It only comes out after dark and sleeps upside down."),
    'louvet': ("Un louveteau curieux qui suit les promeneurs à distance.", "A curious wolf cub that follows walkers from a distance."),
    'esprillon': ("Un petit esprit perdu. Il cherche quelqu'un, mais ne sait plus qui.", "A small lost spirit. It's looking for someone, but no longer knows who."),
    'crapoison': ("Sa peau brille de couleurs vives : un avertissement à ne pas ignorer.", "Its skin shines with bright colors: a warning not to ignore."),
    'carapierre': ("Sa carapace pèse plus lourd que lui. Il avance quand même.", "Its shell weighs more than it does. It keeps going anyway."),
    'givrelin': ("Là où il passe, l'herbe se couvre de givre en dessinant des étoiles.", "Wherever it goes, the grass frosts over in star patterns."),
    'zephyrin': ("Il vole si vite qu'on ne voit que le vent qu'il laisse derrière lui.", "It flies so fast you only see the wind it leaves behind."),
    'ferrolin': ("Il mange de la ferraille et rouille quand il est triste.", "It eats scrap metal and rusts when it's sad."),
    'cristallin': ("La lumière qui le traverse se change en arc-en-ciel.", "Light passing through it turns into a rainbow."),
    'etincelot': ("Il fait des étincelles quand il éternue. Gare aux meules de foin.", "It sparks when it sneezes. Watch out for haystacks."),
    'flammenard': ("Son pelage brûle sans jamais se consumer. Il protège ceux qu'il aime.", "Its fur burns without ever burning away. It protects those it loves."),
    'torrentin': ("Il remonte les cascades pour le plaisir.", "It climbs waterfalls just for fun."),
    'ronceflore': ("Ses fleurs sentent le miel ; ses ronces, beaucoup moins.", "Its flowers smell of honey; its brambles, much less so."),
    'rongecrin': ("Ses dents poussent toute sa vie. Il ronge donc tout, toute sa vie.", "Its teeth grow all its life. So it gnaws everything, all its life."),
    'champignard': ("Un champignon géant. La nuit, son chapeau luit doucement.", "A giant mushroom. At night, its cap glows softly."),
    'lucifee': ("Ses ailes lumineuses éclairent les nuits d'été.", "Its glowing wings light up summer nights."),
    'fournezard': ("Son ventre est un four : les mineurs y cuisent leur pain.", "Its belly is an oven: miners bake their bread in it."),
    'banquisot': ("Il glisse sur le ventre sur la glace, pour aller plus vite.", "It slides on its belly across the ice, to go faster."),
    'serpentox': ("Un seul regard de lui fait frissonner les plus braves.", "A single look from it makes the bravest shiver."),
    'noctaile': ("Elle chasse en silence sous la lune, guidée par les échos.", "It hunts silently under the moon, guided by echoes."),
    'orageon': ("Il attire la foudre exprès, pour se recharger.", "It attracts lightning on purpose, to recharge."),
    'carapic': ("Ses pics de pierre repoussent même les ours.", "Its stone spikes repel even bears."),
    'fulgurin': ("Il traverse le ciel comme un éclair, et laisse une odeur d'orage.", "It crosses the sky like lightning, leaving a smell of storm."),
    'givrenard': ("Son pelage de glace ne fond jamais, même au soleil.", "Its icy fur never melts, even in the sun."),
    'ferrogeant': ("Un géant d'acier, lent mais inébranlable.", "A steel giant, slow but unshakeable."),
    'prismalin': ("Il disperse la lumière en mille éclats. Les mages s'en disputent un.", "It scatters light into a thousand shards. Mages fight over owning one."),
    'sylvarque': ("Le gardien de la forêt. La brume l'avait rendu fou ; il dort, à présent.", "The forest's guardian. The mist had driven it mad; now it sleeps."),
    'golem': ("Fait de suie et de pierre, il gardait la route de Forgeroc.", "Made of soot and stone, it guarded the road to Forgeroc."),
    'ronce_mere': ("La mère de toutes les ronces du marais. Elle a des racines partout.", "The mother of every bramble in the marsh. Its roots are everywhere."),
    'ignarok': ("Le cœur du volcan. Quand il dort, Forgeroc entend les oiseaux.", "The heart of the volcano. When it sleeps, Forgeroc hears the birds."),
    'givrecorne': ("Le gardien des Pics. Sa corne gèle l'air autour de lui.", "The guardian of the Peaks. Its horn freezes the air around it."),
    'brumeloup': ("Un loup fait de brume, qui hurle aux lunes qu'on ne voit pas.", "A wolf made of mist, howling at moons no one can see."),
    'nebulis': ("Un esprit de brume qui chante des berceuses oubliées.", "A mist spirit that sings forgotten lullabies."),
    'orbrume': ("Une pierre qui garde la brume prisonnière. Elle pèse plus lourd la nuit.", "A stone that keeps mist prisoner. It weighs more at night."),
    'mere_brumes': ("Elle pleurait la brume pour protéger le monde de l'orage.", "It wept the mist to protect the world from the storm."),
    'ecumet': ("Il naît de l'écume des vagues et disparaît quand la mer se calme.", "It is born from the foam of the waves and vanishes when the sea calms."),
    'coquillon': ("Il se cache dans sa coquille dès qu'on le regarde trop longtemps.", "It hides in its shell whenever you look at it too long."),
    'bastillon': ("Sa coquille est une forteresse. Des crabes y habitent avec lui.", "Its shell is a fortress. Crabs live inside it with it."),
    'voltanguille': ("Elle donne de petites décharges aux nageurs trop curieux.", "It gives little shocks to swimmers who are too curious."),
    'fulgoray': ("Une raie qui vole au-dessus des vagues les soirs d'orage.", "A ray that flies over the waves on stormy evenings."),
    'nuageon': ("Un petit nuage qui pleure quand on le gronde.", "A little cloud that cries when scolded."),
    'orageline': ("Elle danse dans les éclairs et rit avec le tonnerre.", "It dances in the lightning and laughs with the thunder."),
    'noyelin': ("L'esprit d'un marin perdu, qui cherche encore la côte.", "The spirit of a lost sailor, still looking for the shore."),
    'kraken': ("Le maître des récifs. Ses bras font chavirer les navires.", "The master of the reefs. Its arms capsize ships."),
    'griffon': ("Il garde le pied de la Tour de l'Orage et ne dort jamais.", "It guards the foot of the Storm Tower and never sleeps."),
    'seigneur_orage': ("L'orage vivant que les premiers gardiens avaient enfermé.", "The living storm the first guardians had locked away."),
    'tisseline': ("Ses toiles brillent dans le noir des grottes comme des étoiles.", "Its webs shine in the dark of the caves like stars."),
    'osselet': ("Un petit squelette farceur qui claque des dents pour faire peur.", "A prankster little skeleton that chatters its teeth to scare you."),
    'gobelard': ("Il vole tout ce qui brille et le cache au fond des bois.", "It steals anything shiny and hides it deep in the woods."),
    'brutor': ("Il frappe d'abord et ne réfléchit jamais ensuite.", "It hits first and never thinks afterwards."),
    'draconnet': ("Un bébé dragon. Il crache plus de fumée que de feu.", "A baby dragon. It breathes more smoke than fire."),
    'coffrelin': ("Un coffre qui a des dents. Ne l'ouvrez pas trop vite.", "A chest with teeth. Don't open it too fast."),
    'lueurine': ("Sa lumière turquoise ne s'éteint qu'au lever du jour.", "Its turquoise light only goes out at daybreak."),
    'loup_aube': ("Il hurle au soleil levant pour réveiller l'île.", "It howls at the rising sun to wake the island."),
    'crapale': ("Une grenouille pâle qui vit dans la vase et la lumière.", "A pale frog that lives in mud and light."),
    'aubespectre': ("Un esprit rose qui soigne ceux qu'il aime… et les autres aussi.", "A pink spirit that heals those it loves… and everyone else too."),
    'rocaube': ("Une pierre rose qui garde la chaleur du matin.", "A pink stone that keeps the warmth of morning."),
    'vasepent': ("Il dort sous la vase du marais, un œil toujours ouvert.", "It sleeps under the marsh mud, one eye always open."),
    'aubelle': ("Elle vole d'île en île en portant la lumière de l'aube.", "It flies from island to island carrying the light of dawn."),
    'nacrelin': ("Ses écailles nacrées reflètent la lune comme un miroir.", "Its pearly scales reflect the moon like a mirror."),
    'gardien_lueurs': ("Il veillait sur le Bois des Lueurs et chassait tout ce qui sentait la brume.", "It watched over the Glimmer Wood and chased off anything that smelled of mist."),
    'reine_marais': ("Elle gardait la Larme d'Aube au cœur d'une fleur géante.", "It guarded the Dawn Tear in the heart of a giant flower."),
    'brume_noire': ("Le reste de la brume, devenu colère. Il n'en reste rien.", "What was left of the mist, turned into anger. Nothing of it remains."),
    'troll_mines': ("Il s'était installé au fond des mines, avec ses cailloux préférés.", "It had settled at the bottom of the mines, with its favorite rocks."),
    'colosse_givre': ("Il dormait sous le glacier depuis mille hivers.", "It had slept under the glacier for a thousand winters."),
    'leviathan': ("Le monstre de l'épave. On dit qu'il a coulé le navire par curiosité.", "The monster of the wreck. They say it sank the ship out of curiosity."),
    'orage_ancien': ("Il était là avant les gardiens, avant la brume, avant tout.", "It was there before the guardians, before the mist, before everything."),
    'harpie_givre': ("Elle soufflait le vent glacé sur Givreval, du haut du col.", "It blew the icy wind onto Givreval, from the top of the pass."),
    'gardien_marees': ("Une tortue couverte de corail, qui gardait le trident depuis mille ans.", "A coral-covered turtle that had guarded the trident for a thousand years."),
    'bourrasquin': ("Né dans les rafales du col, il ne tient jamais en place.", "Born in the gusts of the pass, it never stays still."),
    'givroc': ("Une pierre couverte de givre, qui craque quand on la touche.", "A frost-covered stone that cracks when you touch it."),
    'lanternelle': ("Une petite flamme qui éclaire le chemin des esprits.", "A little flame that lights the way for spirits."),
    'fongombre': ("Un champignon d'ombre qui pousse là où personne ne regarde.", "A shadow mushroom that grows where no one looks."),
    'corailleur': ("Il construit des récifs pour les petits poissons.", "It builds reefs for the little fish."),
    'perlin': ("Elle garde une perle sous sa langue et ne la montre jamais.", "It keeps a pearl under its tongue and never shows it."),
    'murenix': ("Une murène qui chante, la nuit, dans les grottes de la mer.", "An eel that sings at night in the sea caves."),
    'petrolin': ("La mousse grise le couvre peu à peu, jusqu'à ce qu'il devienne pierre.", "Grey moss covers it little by little, until it turns to stone."),
    'fossiloup': ("Un loup de pierre. Sa meute garde la forêt pétrifiée.", "A stone wolf. Its pack guards the petrified forest."),
    'echoplume': ("Chacun de ses cris revient trois fois, de plus en plus doux.", "Each of its cries comes back three times, softer and softer."),
    'cendrelys': ("Sa flamme bleue ne brûle pas : elle chante.", "Its blue flame doesn't burn: it sings."),
    'braisaure': ("Il nage dans la lave du cratère comme dans un lac.", "It swims in the crater's lava as if it were a lake."),
    'spectrecho': ("L'écho d'une voix oubliée, devenu une créature.", "The echo of a forgotten voice, turned into a creature."),
    'colosse_echo': ("On le prenait pour un arbre de pierre. Il attendait qu'on le réveille.", "Everyone took it for a stone tree. It was waiting to be woken."),
    'phenix_echo': ("Il renaît de ses cendres, et chante à chaque fois une nouvelle chanson.", "It is reborn from its ashes, and sings a new song each time."),
    'ombre_gardien': ("L'écho du Premier Gardien, perdu dans la brume noire, puis apaisé.", "The echo of the First Guardian, lost in the black mist, then calmed."),
    'veilleur_phare': ("Il garde la lumière du vieux phare depuis mille ans.", "It has guarded the old lighthouse's light for a thousand years."),
}
ESP = json.load(open(D + 'especes.json', encoding='utf-8'))
if any('description' in e for e in ESP):
    sys.exit('Déjà fait.')
missing = [e['id'] for e in ESP if not e.get('humain') and e['id'] not in DESC]
if missing:
    sys.exit('descriptions manquantes : %s' % missing)
out = []
for e in ESP:
    if e['id'] in DESC:
        e2 = {}
        for k, v in e.items():
            e2[k] = v
            if k == 'nom':
                e2['description'] = DESC[e['id']][0]
        e = e2
    out.append(e)
open(D + 'especes.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(out) + '\n')
tr = {fr: en for fr, en in DESC.values()}
json.dump(tr, open(os.path.join(os.environ.get('TRAD', '.'), 'descriptions_en.json'), 'w', encoding='utf-8'), ensure_ascii=False)
print(len(DESC), 'descriptions')
