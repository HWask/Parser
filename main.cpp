#include <fstream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <set>
#include <array>
#include <string>
//todo: change this to queue
#include <deque>

//parser building a DOM-Tree
//parsing the following grammar
//ignoring newline, tab characters
//capabale of verifying the correct format
//this is a context-free grammar, its even LL(k)
//the algorithm is a top down parser
/*
Epsilon is the symbol for empty string

obj -> string '{' D '}'
D -> obj D | keyValue D | Epsilon
keyValue -> key value
key -> string
value -> string
string -> '"' letters '"'
*/
enum TOKEN
{
	STRING_START = '"',
	STRING_END = '"',
	OBJECT_START = '{',
	OBJECT_END = '}',
	FILE_END,
	OBJECT,
	STRING,
	LETTER,
	D,
	KEY,
	VALUE,
	KEYVALUE,
	EPSILON
};

class Node
{
public:
	std::unordered_map<std::string, std::string> keyValues;
	std::vector<std::shared_ptr<Node>> children; //nested objects
	std::string name;
	std::shared_ptr<Node> parent = 0;
};

std::deque<TOKEN> state;
TOKEN GetToken(std::ifstream& file, char& out, bool ignoreWhitespace)
{
	while (!file.eof())
	{
		char c = file.get();
		if (c != '\n' && c != '\t' && ((ignoreWhitespace && c != ' ') || !ignoreWhitespace) )
		{
			//STRING_END is the same as STRING_START and therefore returns here aswell
			if (c == STRING_START)
			{
				out = STRING_START;
				return STRING_START;
			}
			if (c == OBJECT_START)
			{
				out = OBJECT_START;
				return OBJECT_START;
			}
			if (c == OBJECT_END)
			{
				out = OBJECT_END;
				return OBJECT_END;
			}

			out = c;
			return LETTER;
		}
	}

	return FILE_END;
}

TOKEN LookAhead(std::ifstream& f)
{
	auto cur = f.tellg();
	
	char c;
	TOKEN t;
	t = GetToken(f, c, true);
	if (t == FILE_END)
	{
		f.seekg(cur);
		//invalid format
		return FILE_END;
	}
	if (t == OBJECT_END)
	{
		f.seekg(cur);
		return EPSILON;
	}
	if (t == STRING_START)
	{
		while ((t = GetToken(f, c, false)) != FILE_END)
		{
			if (t == STRING_END)
			{
				t = GetToken(f, c, true);
				if (t == FILE_END)
				{
					f.seekg(cur);
					return FILE_END;
				}
				if (t == STRING_START)
				{
					f.seekg(cur);
					return KEYVALUE;
				}
				if (t == OBJECT_START)
				{
					f.seekg(cur);
					return OBJECT;
				}
			}
		}
		//invalid format
		f.seekg(cur);
		return FILE_END;
	}

	f.seekg(cur);
	//invalid format
	return FILE_END;
}

//Parses the file and builds a DOM-Tree. Verifies the correct
//file format aswell. Returns TRUE is file format is correct
bool Parse(std::string file, std::shared_ptr<Node>& DOM)
{
	std::ifstream f(file);
	//todo: check whether file is empty excluding whitespaces
	if (!f.is_open())
		return false;

	bool isObjectName = false;
	bool isKeyName = false;
	bool isValName = false;
	char c;
	TOKEN t;
	std::string s;
	std::string key;
	std::string val;
	std::shared_ptr<Node> n(new Node);
	n->name = "root";
	DOM = n;

	state.push_front(OBJECT);
LOOP:while (!state.empty())
	{
		auto front = state.front();
		state.pop_front();

		if (front == OBJECT)
		{
			//DOM stuff
			isObjectName = true;
			std::shared_ptr<Node> child(new Node);
			child->parent = n;
			n->children.push_back(child);
			n = child;

			state.push_front(OBJECT_END);
			state.push_front(D);
			state.push_front(OBJECT_START);
			state.push_front(STRING);
		}
		if (front == STRING)
		{
			state.push_front(STRING_END);
			state.push_front(STRING_START);
		}
		if (front == D)
		{
			//figure out what D is either an Object
			//or KeyValue or empty string (epsilon)
			TOKEN t2 = LookAhead(f);
			if (t2 == OBJECT)
			{
				state.push_front(D);
				state.push_front(OBJECT);
			}
			if (t2 == KEYVALUE)
				state.push_front(KEYVALUE);
			if (t2 == EPSILON)
				continue;
			if (t2 == FILE_END)
			{
				//invalid format
				return false;
			}
		}
		if (front == KEYVALUE)
		{
			state.push_front(VALUE);
			state.push_front(KEY);
		}
		if (front == KEY)
		{
			//DOM stuff
			isKeyName = true;

			state.push_front(STRING);
		}
		if (front == VALUE)
		{
			//DOM stuff
			isValName = true;

			state.push_front(D);
			state.push_front(STRING);
		}
		if (front == STRING_START)
		{
			//read string
			t = GetToken(f, c, true);
			if (t == STRING_START)
			{
				s.clear();
				while ((t=GetToken(f, c, false)) != FILE_END)
				{
					if (t != STRING_END)
					{
						s += c;
					}
					else
					{
						auto front2 = state.front();
						if (front2 != STRING_END)
							return false; //string_end expected should never happen
						else
							state.pop_front();

						//DOM stuff
						if (isObjectName)
						{
							n->name = s;
							isObjectName = false;
						}

						if (isKeyName)
						{
							key = s;
							isKeyName = false;
						}

						if (isValName)
						{
							val = s;
							n->keyValues[key] = val;
							isValName = false;
						}

						goto LOOP;
					}
				}
				//no ending terminator for string
				return false;
			}
			else
				return false;
		}
		if (front == OBJECT_START)
		{
			t = GetToken(f, c, true);
			if (t != OBJECT_START || t == FILE_END)
			{
				return false;
			}
		}
		if (front == OBJECT_END)
		{
			//DOM stuff
			//travel up 1 level in the tree
			n = n->parent;

			t = GetToken(f, c, true);
			if (t != OBJECT_END || t == FILE_END)
			{
				return false;
			}
		}
	}

	return true;
}

//Breadth-first search, since my shit was at a low level of the tree
std::shared_ptr<Node> FindObjectByName(std::shared_ptr<Node> anchor, std::string objName)
{
	std::deque<std::shared_ptr<Node>> queue;
	queue.push_back(anchor);

	while (!queue.empty())
	{
		if (queue.front()->name == objName)
			return queue.front();

		for (auto child : queue.front()->children)
			queue.push_back(child);

		queue.pop_front();
	}

	return 0;
}

struct skinInfo
{
	int seed = -1;
	int paintkit;
};

std::vector<std::string> splitString(std::string str, char delim)
{
	size_t pos2, pos = 0;
	std::vector<std::string> strings;
	do
	{
		pos2 = str.find_first_of(delim, pos);
		if (pos2 == std::string::npos)
			strings.push_back(str.substr(pos, std::string::npos));
		else
			strings.push_back(str.substr(pos, pos2 - pos));

		pos = pos2 + 1;
	} while (pos2 != std::string::npos);

	return strings;
}

int main()
{
	/*
	std::string test = "a b c def gh l  ";
	auto strings = splitString(test, ' ');
	*/

	std::shared_ptr<Node> DOM;
	bool ret = Parse("items_game.txt", DOM);

	auto weaponSkinCombo = FindObjectByName(DOM, "weapon_icons");
	if (!weaponSkinCombo)
		return 0;

	auto skinData = FindObjectByName(DOM, "paint_kits");
	if (!skinData)
		return 0;

	std::unordered_map<std::string, std::set<std::string>> weaponSkins;
	std::unordered_map<std::string, skinInfo> skinMap;

	std::array<std::string, 43> weaponNames = {
		"deagle",
		"elite",
		"fiveseven",
		"glock",
		"ak47",
		"aug",
		"awp",
		"famas",
		"g3sg1",
		"galilar",
		"m249",
		"m4a1_silencer", //needs to be before m4a1 else silencer doesnt get filtered out :D
		"m4a1",
		"mac10",
		"p90",
		"ump45",
		"xm1014",
		"bizon",
		"mag7",
		"negev",
		"sawedoff",
		"tec9",
		"hkp2000",
		"mp7",
		"mp9",
		"nova",
		"p250",
		"scar20",
		"sg556",
		"ssg08",
		"usp_silencer",
		"cz75a",
		"revolver",
		"knife_m9_bayonet", //needs to be before bayonet else knife_m9_bayonet doesnt get filtered out :D
		"bayonet",
		"knife_flip",
		"knife_gut",
		"knife_karambit",
		"knife_tactical",
		"knife_falchion",
		"knife_survival_bowie",
		"knife_butterfly",
		"knife_push"
	};

	//populate weaponSkins
	for (auto child : weaponSkinCombo->children)
	{
		for (auto weapon : weaponNames)
		{
			auto skinName = child->keyValues["icon_path"];
			auto pos = skinName.find(weapon);
			//filter out the skinname
			if (pos != std::string::npos)
			{ 
				auto pos2 = skinName.find_last_of('_');
				weaponSkins[weapon].insert(
					skinName.substr(pos + weapon.length() + 1,
					pos2 - pos - weapon.length() - 1)
				);
				break;
			}
		}
	}

	//populate skinData
	for (auto skin : skinData->children)
	{
		skinInfo si;
		si.paintkit = std::stoi(skin->name);

		if (skin->keyValues.count("seed") != 0)
			si.seed = std::stoi(skin->keyValues["seed"]);

		skinMap[skin->keyValues["name"]] = si;
	}

	//dump skins
	std::ofstream outfile("dump.txt");
	for (auto weapon : weaponNames)
	{
		outfile << weapon << std::endl;
		for (auto skin : weaponSkins[weapon])
		{
			outfile << "\t" << skin << std::endl;
		}
	}

	return 0;
}