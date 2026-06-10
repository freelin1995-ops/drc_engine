#include "mpi/script_analyzer.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <set>

namespace drc {

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::pair<std::string, std::string> split_last_method_call(const std::string& expr) {
    int depth = 0;
    int last_colon = -1;
    int paren_start = -1;
    for (int i = static_cast<int>(expr.size()) - 1; i >= 0; i--) {
        if (expr[i] == ')') depth++;
        else if (expr[i] == '(') {
            depth--;
            if (depth == 0) {
                paren_start = i;
                for (int j = i - 1; j >= 0; j--) {
                    if (expr[j] == ':') {
                        last_colon = j;
                        break;
                    }
                }
                break;
            }
        }
    }
    if (last_colon >= 0 && paren_start > last_colon) {
        return {expr.substr(0, last_colon), expr.substr(last_colon + 1)};
    }
    return {"", ""};
}

ScriptAnalyzer::ScriptAnalyzer(const std::string& script_path) {
    std::ifstream file(script_path);
    if (!file.is_open()) {
        m_original_script = "";
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    m_original_script = buffer.str();
    split_lines(m_original_script);
    m_valid = true;
}

void ScriptAnalyzer::split_lines(const std::string& script) {
    m_original_lines.clear();
    std::stringstream ss(script);
    std::string line;
    while (std::getline(ss, line)) {
        std::string trimmed = trim(line);
        if (is_comment_or_empty(trimmed)) continue;
        LineInfo li;
        li.text = trimmed;
        m_original_lines.push_back(li);
    }
}

bool ScriptAnalyzer::is_comment_or_empty(const std::string& line) const {
    return line.empty() || line.find("--") == 0;
}

std::string ScriptAnalyzer::strip_local(const std::string& line) {
    std::regex local_re(R"(^\s*local\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=)");
    std::smatch m;
    if (std::regex_search(line, m, local_re)) {
        return line.substr(0, m.position(0)) +
               m.str(1) + " =" +
               line.substr(m.position(0) + m.length(0));
    }
    return line;
}

std::string ScriptAnalyzer::extract_assigned_var(const std::string& line) const {
    std::regex assign_re(R"(^\s*(?:local\s+)?([a-zA-Z_][a-zA-Z0-9_]*)\s*=)");
    std::smatch m;
    if (std::regex_search(line, m, assign_re)) {
        return m.str(1);
    }
    return "";
}

std::vector<std::string> ScriptAnalyzer::extract_referenced_vars(const std::string& expr) const {
    std::vector<std::string> result;
    std::regex var_re(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\b)");
    static const std::set<std::string> keywords = {
        "local", "input", "source", "target", "write",
        "return", "nil", "true", "false"
    };
    auto it = std::sregex_iterator(expr.begin(), expr.end(), var_re);
    auto end = std::sregex_iterator();
    for (; it != end; ++it) {
        std::string var = it->str(1);
        if (keywords.find(var) == keywords.end()) {
            result.push_back(var);
        }
    }
    return result;
}

bool ScriptAnalyzer::is_source_line(const std::string& line) const {
    return line.find("source(") == 0 || line.find("source (") == 0;
}

bool ScriptAnalyzer::is_target_line(const std::string& line) const {
    return line.find("target(") == 0 || line.find("target (") == 0;
}

bool ScriptAnalyzer::is_write_line(const std::string& line) const {
    return line.find("write(") == 0 || line.find("write (") == 0;
}

bool ScriptAnalyzer::is_output_line(const std::string& line) const {
    return line.find("output(") == 0 || line.find("output (") == 0 || line.find(":output(") != std::string::npos;
}

bool ScriptAnalyzer::is_input_call(const std::string& line) const {
    auto eq_pos = line.find('=');
    if (eq_pos == std::string::npos) {
        return trim(line).find("input(") == 0 || trim(line).find("input (") == 0;
    }
    std::string rhs = trim(line.substr(eq_pos + 1));
    return rhs.find("input(") == 0 || rhs.find("input (") == 0;
}

std::string ScriptAnalyzer::decompose_chains(const std::string& line) {
    auto eq_pos = line.find('=');
    if (eq_pos == std::string::npos) {
        return line;
    }

    std::string lhs = trim(line.substr(0, eq_pos));
    std::string rhs = trim(line.substr(eq_pos + 1));

    std::vector<std::string> methods;
    std::string base = rhs;
    while (true) {
        auto [obj, method] = split_last_method_call(base);
        if (obj.empty()) break;
        base = obj;
        methods.push_back(method);
    }

    if (methods.empty()) {
        return line;
    }

    std::reverse(methods.begin(), methods.end());

    std::string base_temp = "__t" + std::to_string(++m_temp_counter);
    {
        LineInfo base_li;
        base_li.text = base_temp + " = " + base;
        base_li.assigned_var = base_temp;
        base_li.is_input = (base.find("input(") == 0 || base.find("input (") == 0);
        m_lines.push_back(base_li);
    }

    std::string receiver = base_temp;
    for (size_t j = 0; j < methods.size(); j++) {
        bool is_last = (j == methods.size() - 1);
        std::string target = is_last ? lhs : ("__t" + std::to_string(++m_temp_counter));

        LineInfo li;
        li.text = target + " = " + receiver + ":" + methods[j];
        li.assigned_var = target;
        m_lines.push_back(li);

        receiver = target;
    }

    return "";
}

void ScriptAnalyzer::normalize() {
    m_lines.clear();
    m_lines_str.clear();
    m_temp_counter = 0;

    for (size_t i = 0; i < m_original_lines.size(); i++) {
        std::string line = m_original_lines[i].text;

        if (is_source_line(line) || is_target_line(line) ||
            is_write_line(line) || is_output_line(line)) {
            LineInfo li;
            li.text = line;
            li.assigned_var = extract_assigned_var(line);
            m_lines.push_back(li);
            continue;
        }

        line = strip_local(line);

        if (line.find('=') != std::string::npos) {
            std::string result = decompose_chains(line);

            if (!result.empty()) {
                LineInfo li;
                li.text = result;
                li.assigned_var = extract_assigned_var(result);
                li.is_input = is_input_call(result);
                m_lines.push_back(li);
            }
        } else {
            LineInfo li;
            li.text = line;
            m_lines.push_back(li);
        }
    }

    build_normalized();
    m_normalized = true;
}

void ScriptAnalyzer::build_normalized() {
    m_normalized_script.clear();

    for (size_t i = 0; i < m_lines.size(); i++) {
        const auto& li = m_lines[i];

        if (!li.assigned_var.empty() && !li.is_input) {
            auto eq_pos = li.text.find('=');
            if (eq_pos != std::string::npos) {
                std::string rhs = li.text.substr(eq_pos + 1);
                rhs = trim(rhs);
                m_normalized_script += "__expr(\"" + rhs + "\")\n";
            }
        }

        m_normalized_script += li.text + "\n";
    }

    m_lines_str.clear();
    for (const auto& li : m_lines) {
        m_lines_str.push_back(li.text);
    }
}

void ScriptAnalyzer::build_ref_table() {
    if (!m_normalized) normalize();
    m_ref_table.clear();

    for (int i = 0; i < static_cast<int>(m_lines.size()); i++) {
        const auto& li = m_lines[i];
        if (li.assigned_var.empty())
            continue;

        RefEntry entry;
        entry.var = li.assigned_var;
        entry.def_line = i;
        entry.is_input = li.is_input;

        for (int j = i + 1; j < static_cast<int>(m_lines.size()); j++) {
            auto refs = extract_referenced_vars(m_lines[j].text);
            for (const auto& ref : refs) {
                if (ref == li.assigned_var) {
                    entry.ref_lines.push_back(j);
                }
            }
        }

        m_ref_table[li.assigned_var] = entry;
    }
}

bool ScriptAnalyzer::has_downstream_refs(const std::string& var, int def_line) const {
    auto it = m_ref_table.find(var);
    if (it == m_ref_table.end())
        return false;
    const auto& entry = it->second;
    if (def_line == -1)
        return !entry.ref_lines.empty();
    for (int ref : entry.ref_lines) {
        if (ref > def_line)
            return true;
    }
    return false;
}

bool ScriptAnalyzer::is_input_line(int line_num) const {
    if (line_num < 0 || line_num >= static_cast<int>(m_lines.size()))
        return false;
    return m_lines[line_num].is_input;
}

std::string ScriptAnalyzer::get_assigned_var(int line_num) const {
    if (line_num < 0 || line_num >= static_cast<int>(m_lines.size()))
        return "";
    return m_lines[line_num].assigned_var;
}

} // namespace drc
