#pragma once

#include "Definitions.h"
#include "Includes.h"
#include "LCU.h"
#include "Utils.h"

class GameTab
{
public:
    static void Render()
    {
        if (ImGui::BeginTabItem("Game"))
        {
            static std::string result;
            static std::vector<std::string> itemsMultiSearch = {
                "OP.GG", "U.GG", "PORO.GG", "Porofessor.gg"
            };
            const char* selectedMultiSearch = itemsMultiSearch[S.gameTab.indexMultiSearch].c_str();

            if (ImGui::Button("Multi-Search"))
            {
                result = MultiSearch(itemsMultiSearch[S.gameTab.indexMultiSearch]);
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(static_cast<float>(S.Window.width / 6));
            if (ImGui::BeginCombo("##comboMultiSearch", selectedMultiSearch, 0))
            {
                for (size_t n = 0; n < itemsMultiSearch.size(); n++)
                {
                    const bool is_selected = (S.gameTab.indexMultiSearch == n);
                    if (ImGui::Selectable(itemsMultiSearch[n].c_str(), is_selected))
                        S.gameTab.indexMultiSearch = n;

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Columns(1);
            ImGui::Separator();

            static Json::StreamWriterBuilder wBuilder;
            static std::string sResultJson;
            static char* cResultJson;

            if (!result.empty())
            {
                Json::CharReaderBuilder builder;
                const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
                JSONCPP_STRING err;
                Json::Value root;
                if (!reader->parse(result.c_str(), result.c_str() + static_cast<int>(result.length()), &root, &err))
                    sResultJson = result;
                else
                {
                    sResultJson = Json::writeString(wBuilder, root);
                }
                result = "";
            }

            if (!sResultJson.empty())
            {
                cResultJson = sResultJson.data();
                ImGui::InputTextMultiline("##gameResult", cResultJson, sResultJson.size() + 1, ImVec2(600, 300));
            }

            ImGui::EndTabItem();
        }
    }

    static std::string MultiSearch(const std::string& website)
    {
        std::string names;
        std::string champSelect = LCU::Request("GET", "https://127.0.0.1/lol-champ-select/v1/session");
        if (!champSelect.empty() && champSelect.find("RPC_ERROR") == std::string::npos)
        {
            Json::CharReaderBuilder builder;
            const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
            JSONCPP_STRING err;
            Json::Value rootRegion;
            Json::Value rootCSelect;
            Json::Value rootSummoner;
            Json::Value rootPartcipants;

            std::wstring summNames;
            bool isRanked = false;

            if (reader->parse(champSelect.c_str(), champSelect.c_str() + static_cast<int>(champSelect.length()), &rootCSelect, &err))
            {
                auto teamArr = rootCSelect["myTeam"];
                if (teamArr.isArray())
                {
                    for (auto& i : teamArr)
                    {
                        if (i["nameVisibilityType"].asString() == "HIDDEN")
                        {
                            isRanked = true;
                            break;
                        }

                        std::string summId = i["summonerId"].asString();
                        if (summId != "0")
                        {
                            std::string summoner = LCU::Request("GET", "https://127.0.0.1/lol-summoner/v1/summoners/" + summId);
                            if (reader->parse(summoner.c_str(), summoner.c_str() + static_cast<int>(summoner.length()), &rootSummoner, &err))
                            {
                                summNames += Utils::StringToWstring(rootSummoner["gameName"].asString()) + L"%23" + Utils::StringToWstring(rootSummoner["tagLine"].asString()) + L",";
                            }
                        }
                    }

                    // Ranked Lobby Reveal
                    if (isRanked)
                    {
                        summNames = L"";

                        LCU::SetCurrentClientRiotInfo();
                        std::string participants = cpr::Get(
                            cpr::Url{ std::format("https://127.0.0.1:{}/chat/v5/participants", LCU::riot.port) },
                            cpr::Header{ Utils::StringToHeader(LCU::riot.header) }, cpr::VerifySsl{ false }).text;
                        if (reader->parse(participants.c_str(), participants.c_str() + static_cast<int>(participants.length()), &rootPartcipants,
                            &err))
                        {
                            auto participantsArr = rootPartcipants["participants"];
                            if (participantsArr.isArray())
                            {
                                for (auto& i : participantsArr)
                                {
                                    if (!i["cid"].asString().contains("champ-select"))
                                        continue;
                                    summNames += Utils::StringToWstring(i["game_name"].asString()) + L"%23" + Utils::StringToWstring(i["game_tag"].asString()) + L",";
                                }
                            }
                        }
                    }

                    std::wstring region;
                    if (website == "U.GG") // platformId (euw1, eun1, na1)
                    {
                        std::string getAuthorization = LCU::Request("GET", "/lol-rso-auth/v1/authorization");
                        if (reader->parse(getAuthorization.c_str(), getAuthorization.c_str() + static_cast<int>(getAuthorization.length()),
                            &rootRegion, &err))
                        {
                            region = Utils::StringToWstring(rootRegion["currentPlatformId"].asString());
                        }
                    }
                    else // region code (euw, eune na)
                    {
                        std::string getRegion = LCU::Request("GET", "/riotclient/region-locale");
                        if (reader->parse(getRegion.c_str(), getRegion.c_str() + static_cast<int>(getRegion.length()), &rootRegion, &err))
                        {
                            region = Utils::StringToWstring(rootRegion["webRegion"].asString());
                        }
                    }

                    if (!region.empty())
                    {
                        if (summNames.empty())
                            return "Failed to get summoner names";

                        if (summNames.at(summNames.size() - 1) == L',')
                            summNames.pop_back();

                        std::wstring url;
                        if (website == "OP.GG")
                        {
                            url = L"https://" + region + L".op.gg/multi/query=" + summNames;
                        }
                        else if (website == "U.GG")
                        {
                            url = L"https://u.gg/multisearch?summoners=" + summNames + L"&region=" + Utils::ToLower(region);
                        }
                        else if (website == "PORO.GG")
                        {
                            url = L"https://poro.gg/multi?region=" + Utils::ToUpper(region) + L"&q=" + summNames;
                        }
                        else if (website == "Porofessor.gg")
                        {
                            url = L"https://porofessor.gg/pregame/" + region + L"/" + summNames + L"/soloqueue/season";
                        }
                        Utils::OpenUrl(url.c_str(), nullptr, SW_SHOW);
                        return Utils::WstringToString(url);
                    }
                    return "Failed to get region";
                }
            }
        }

        return "Champion select not found";
    }
};
