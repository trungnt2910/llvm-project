#include <cassert>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>

struct Version
{
  std::string name;
  std::vector<std::string> symbols;
  std::string depends;
};

std::string trim(const std::string& source)
{
  auto begin = source.begin();
  auto end = source.end();

  while (begin < end && std::isspace(*begin))
  {
    ++begin;
  }

  while (end > begin && std::isspace(*std::prev(end)))
  {
    --end;
  }

  return std::string(begin, end);
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cout   << "Usage: "
                    << argv[0]
                    << " <input version script> <output verion script>"
                    << std::endl;
        return 1;
    }

    const char* inputFile = argv[1];
    const char* outputFile = argv[2];

    std::ifstream fin(inputFile);
    std::ofstream fout(outputFile);

    bool insideVersion = false;
    bool endingVersion = false;
    bool beginningVersion = false;

    std::string currentLine;

    std::unordered_map<std::string, Version> versions;

    Version* currentVersion = nullptr;

    while (fin)
    {
      std::getline(fin, currentLine);
      currentLine = trim(currentLine);

      if (currentLine[0] == '#')
      {
        continue;
      }

      std::stringstream lin(currentLine);

      while (lin)
      {
        std::string currentToken;
        lin >> currentToken;

        if (currentToken.empty())
        {
          continue;
        }

        if (!insideVersion && !beginningVersion)
        {
          // Next token should be a version.
          currentVersion = &versions[currentToken];
          currentVersion->name = std::move(currentToken);
          beginningVersion = true;
        }
        else if (!insideVersion && beginningVersion)
        {
          // Next token should be a brace.
          assert(currentToken == "{");
          insideVersion = true;
          beginningVersion = false;
        }
        else if (insideVersion)
        {
          if (currentToken[0] == '}')
          {
            // Getting out of the current version.
            insideVersion = false;
            if (currentToken == "};")
            {
              // End this one for good.
              currentVersion = nullptr;
              endingVersion = false;
            }
            else
            {
              // Still waiting for something.
              endingVersion = true;
            }
          }
          else
          {
            // Put whatever we have in the vector.
            currentVersion->symbols.emplace_back(std::move(currentToken));
          }
        }
        else if (!insideVersion && endingVersion)
        {
          // Expecting either a token or a name.
          if (currentToken == ";")
          {
            // End this one for good.
            currentVersion = nullptr;
            endingVersion = false;
          }
          else if (currentToken.back() == ';')
          {
            currentToken.pop_back();
            currentVersion->depends = std::move(currentToken);
            // End this one for good.
            currentVersion = nullptr;
            endingVersion = false;
          }
          else
          {
            currentVersion->depends = std::move(currentToken);
          }
        }
      }
    }

    fin.close();

    for (auto & kvp : versions)
    {
      currentVersion = &kvp.second;

      fout << currentVersion->name << " { ";
      for (const auto & sym : currentVersion->symbols)
      {
        fout << sym << " ";
      }
      fout << "}" << ((currentVersion->depends.length()) ? " " + currentVersion->depends : "") << ";\n";
    }

    fout.close();

    return 0;
}