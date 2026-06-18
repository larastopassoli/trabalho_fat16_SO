

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <ctime>

using namespace std;

//  pack(1) para o C++ não colocar espaços extras dentro das structs.
// Isso é importante porque as structs precisam bater exatamente com o formato gravado na imagem FAT16.
#pragma pack(push, 1)

//  o Boot Sector da FAT16.
// Ele fica no começo da imagem e guarda informações como bytes por setor, quantidade de FATs e tamanho da FAT.
struct BootSector {
    uint8_t jump[3];
    char oem[8];
    uint16_t bytesPorSetor;
    uint8_t setoresPorCluster;
    uint16_t setoresReservados;
    uint8_t numeroDeFATs;
    uint16_t entradasDiretorioRaiz;
    uint16_t totalSetores16;
    uint8_t tipoMidia;
    uint16_t setoresPorFAT;
    uint16_t setoresPorTrilha;
    uint16_t numeroCabecas;
    uint32_t setoresOcultos;
    uint32_t totalSetores32;
    uint8_t numeroDrive;
    uint8_t reservado1;
    uint8_t assinaturaBoot;
    uint32_t numeroSerie;
    char rotuloVolume[11];
    char tipoSistema[8];
};


struct EntradaDiretorio {
    char nome[8];
    char extensao[3];
    uint8_t atributos;
    uint8_t reservadoNT;
    uint8_t tempoCriacaoDecimos;
    uint16_t horaCriacao;
    uint16_t dataCriacao;
    uint16_t dataUltimoAcesso;
    uint16_t clusterAlto;
    uint16_t horaModificacao;
    uint16_t dataModificacao;
    uint16_t primeiroCluster;
    uint32_t tamanhoArquivo;
};

#pragma pack(pop)

// Remove os espaços usados pelo padrão FAT 8.3.
string limparEspacos(const string& texto) {
    size_t inicio = texto.find_first_not_of(' ');
    size_t fim = texto.find_last_not_of(' ');

    if (inicio == string::npos) {
        return "";
    }

    return texto.substr(inicio, fim - inicio + 1);
}

// Monta o nome completo do arquivo juntando nome + extensão.
string montarNomeArquivo(const EntradaDiretorio& entrada) {
    string nome(entrada.nome, 8);
    string extensao(entrada.extensao, 3);

    nome = limparEspacos(nome);
    extensao = limparEspacos(extensao);

    if (!extensao.empty()) {
        return nome + "." + extensao;
    }

    return nome;
}

// Converte para maiúsculo 
// Assim a busca funciona mesmo se o usuário digitar teste.txt.
string converterParaMaiusculo(string texto) {
    transform(texto.begin(), texto.end(), texto.begin(),
              [](unsigned char c) {
                  return static_cast<char>(toupper(c));
              });

    return texto;
}

// Verifico se a entrada do diretório realmente representa um arquivo comum.
// Ignoro entrada vazia, arquivo apagado, nome longo, rótulo de volume e diretórios.
bool entradaValida(const EntradaDiretorio& entrada) {
    unsigned char primeiroByte = static_cast<unsigned char>(entrada.nome[0]);

    // 0x00 indica que nao ha mais entradas no diretorio
    if (primeiroByte == 0x00) {
        return false;
    }

    // 0xE5 indica arquivo apagado
    if (primeiroByte == 0xE5) {
        return false;
    }

    // 0x0F indica entrada de nome longo, que esta implementacao ignora
    if (entrada.atributos == 0x0F) {
        return false;
    }

    // 0x08 indica rotulo de volume, nao e arquivo comum
    if (entrada.atributos & 0x08) {
        return false;
    }

    // 0x10 indica diretorio. O trabalho nao considera subdiretorios
    if (entrada.atributos & 0x10) {
        return false;
    }

    return true;
}

// Lê o Boot Sector diretamente do início da imagem.
// Com esses dados é possivel calcular onde ficam a FAT, o diretório raiz e a área de dados.
BootSector lerBootSector(fstream& imagem) {
    BootSector boot;

    imagem.clear();
    imagem.seekg(0, ios::beg);
    imagem.read(reinterpret_cast<char*>(&boot), sizeof(BootSector));

    return boot;
}

// Calcula o byte inicial do diretório raiz.
// O diretório raiz vem depois dos setores reservados e das cópias da FAT.
uint32_t calcularInicioDiretorioRaiz(const BootSector& boot) {
    uint32_t setorInicioDiretorioRaiz = boot.setoresReservados +
                                        (boot.numeroDeFATs * boot.setoresPorFAT);

    return setorInicioDiretorioRaiz * boot.bytesPorSetor;
}

// Calcula quantos setores o diretório raiz ocupa.
// Cada entrada do diretório possui 32 bytes.
uint32_t calcularSetoresDiretorioRaiz(const BootSector& boot) {
    uint32_t tamanhoDiretorioRaiz = boot.entradasDiretorioRaiz * sizeof(EntradaDiretorio);

    return (tamanhoDiretorioRaiz + boot.bytesPorSetor - 1) / boot.bytesPorSetor;
}

// Calcula onde começa a área de dados.
// É nessa área que fica o conteúdo real dos arquivos.
uint32_t calcularInicioAreaDados(const BootSector& boot) {
    uint32_t setoresDiretorioRaiz = calcularSetoresDiretorioRaiz(boot);

    uint32_t setorInicioAreaDados = boot.setoresReservados +
                                    (boot.numeroDeFATs * boot.setoresPorFAT) +
                                    setoresDiretorioRaiz;

    return setorInicioAreaDados * boot.bytesPorSetor;
}

// Converte um número de cluster para o offset em bytes dentro da imagem.
// Na FAT, os clusters de dados começam a partir do número 2.
uint32_t calcularOffsetCluster(const BootSector& boot, uint16_t cluster) {
    uint32_t inicioAreaDados = calcularInicioAreaDados(boot);
    uint32_t tamanhoCluster = boot.bytesPorSetor * boot.setoresPorCluster;

    return inicioAreaDados + ((cluster - 2) * tamanhoCluster);
}

// total de setores da imagem.
// Em FAT16 ele pode estar no campo de 16 bits ou no campo de 32 bits.
uint32_t calcularTotalSetores(const BootSector& boot) {
    if (boot.totalSetores16 != 0) {
        return boot.totalSetores16;
    }

    return boot.totalSetores32;
}

// Calcula quantos clusters existem na área de dados.
// para saber até onde posso procurar clusters livres.
uint32_t calcularTotalClusters(const BootSector& boot) {
    uint32_t totalSetores = calcularTotalSetores(boot);
    uint32_t setoresDiretorioRaiz = calcularSetoresDiretorioRaiz(boot);

    uint32_t setoresAntesDados = boot.setoresReservados +
                                  (boot.numeroDeFATs * boot.setoresPorFAT) +
                                  setoresDiretorioRaiz;

    uint32_t setoresAreaDados = totalSetores - setoresAntesDados;

    return setoresAreaDados / boot.setoresPorCluster;
}

// Lê o valor de uma posição da FAT.
// Esse valor mostra se o cluster está livre, aponta para outro cluster ou marca fim de arquivo.
uint16_t lerValorFAT16(fstream& imagem, const BootSector& boot, uint16_t cluster) {
    uint32_t inicioFAT = boot.setoresReservados * boot.bytesPorSetor;
    uint32_t offset = inicioFAT + (cluster * 2);

    uint16_t valor = 0;

    imagem.clear();
    imagem.seekg(offset, ios::beg);
    imagem.read(reinterpret_cast<char*>(&valor), sizeof(uint16_t));

    return valor;
}

// Escreve um valor na FAT.
// Como a imagem pode ter mais de uma cópia da FAT, atualizo todas elas.
void escreverValorFAT16EmTodasFATs(
    fstream& imagem,
    const BootSector& boot,
    uint16_t cluster,
    uint16_t valor
) {
    for (int fat = 0; fat < boot.numeroDeFATs; fat++) {
        uint32_t inicioFAT = (boot.setoresReservados + (fat * boot.setoresPorFAT)) * boot.bytesPorSetor;
        uint32_t offset = inicioFAT + (cluster * 2);

        imagem.clear();
        imagem.seekp(offset, ios::beg);
        imagem.write(reinterpret_cast<char*>(&valor), sizeof(uint16_t));
    }

    imagem.flush();
}

// Procura um arquivo pelo nome dentro do diretório raiz.
// Retorna a entrada encontrada para usar nas operações de leitura e atributos.
bool buscarArquivo(fstream& imagem, const BootSector& boot, const string& nomeProcurado, EntradaDiretorio& entradaEncontrada) {
    uint32_t inicioDiretorioRaiz = calcularInicioDiretorioRaiz(boot);

    imagem.clear();
    imagem.seekg(inicioDiretorioRaiz, ios::beg);

    string nomeBusca = converterParaMaiusculo(nomeProcurado);

    for (int i = 0; i < boot.entradasDiretorioRaiz; i++) {
        EntradaDiretorio entrada;

        imagem.read(reinterpret_cast<char*>(&entrada), sizeof(EntradaDiretorio));

        unsigned char primeiroByte = static_cast<unsigned char>(entrada.nome[0]);

        if (primeiroByte == 0x00) {
            break;
        }

        if (entradaValida(entrada)) {
            string nomeArquivo = converterParaMaiusculo(montarNomeArquivo(entrada));

            if (nomeArquivo == nomeBusca) {
                entradaEncontrada = entrada;
                return true;
            }
        }
    }

    return false;
}

// Procura um arquivo e também guarda o offset da entrada no diretório.
// Esse offset é necessário para operações que alteram a entrada, como renomear e apagar.
bool buscarArquivoComOffset(
    fstream& imagem,
    const BootSector& boot,
    const string& nomeProcurado,
    EntradaDiretorio& entradaEncontrada,
    uint32_t& offsetEntrada
) {
    uint32_t inicioDiretorioRaiz = calcularInicioDiretorioRaiz(boot);

    imagem.clear();

    string nomeBusca = converterParaMaiusculo(nomeProcurado);

    for (int i = 0; i < boot.entradasDiretorioRaiz; i++) {
        uint32_t offsetAtual = inicioDiretorioRaiz + (i * sizeof(EntradaDiretorio));

        EntradaDiretorio entrada;

        imagem.seekg(offsetAtual, ios::beg);
        imagem.read(reinterpret_cast<char*>(&entrada), sizeof(EntradaDiretorio));

        unsigned char primeiroByte = static_cast<unsigned char>(entrada.nome[0]);

        if (primeiroByte == 0x00) {
            break;
        }

        if (entradaValida(entrada)) {
            string nomeArquivo = converterParaMaiusculo(montarNomeArquivo(entrada));

            if (nomeArquivo == nomeBusca) {
                entradaEncontrada = entrada;
                offsetEntrada = offsetAtual;
                return true;
            }
        }
    }

    return false;
}

// Valida e formata o nome no padrão FAT 8.3.
// O nome pode ter até 8 caracteres e a extensão até 3.
bool nomeValidoFAT83(const string& nomeArquivo, char nomeFormatado[8], char extensaoFormatada[3]) {
    string nome = converterParaMaiusculo(nomeArquivo);

    size_t posPonto = nome.find('.');

    if (posPonto == string::npos) {
        cout << "Erro: o nome deve ter extensao. Exemplo: NOVO.TXT\n";
        return false;
    }

    if (nome.find('.', posPonto + 1) != string::npos) {
        cout << "Erro: o nome deve ter apenas um ponto. Exemplo: NOVO.TXT\n";
        return false;
    }

    string parteNome = nome.substr(0, posPonto);
    string parteExtensao = nome.substr(posPonto + 1);

    if (parteNome.empty() || parteNome.length() > 8) {
        cout << "Erro: o nome deve ter de 1 a 8 caracteres.\n";
        return false;
    }

    if (parteExtensao.empty() || parteExtensao.length() > 3) {
        cout << "Erro: a extensao deve ter de 1 a 3 caracteres.\n";
        return false;
    }

    string caracteresInvalidos = "\\/:*?\"<>|";

    for (char c : nome) {
        if (caracteresInvalidos.find(c) != string::npos) {
            cout << "Erro: o nome contem caractere invalido para FAT16.\n";
            return false;
        }
    }

    memset(nomeFormatado, ' ', 8);
    memset(extensaoFormatada, ' ', 3);

    memcpy(nomeFormatado, parteNome.c_str(), parteNome.length());
    memcpy(extensaoFormatada, parteExtensao.c_str(), parteExtensao.length());

    return true;
}

// Converte a data gravada 
string formatarDataFAT(uint16_t data) {
    int ano = ((data >> 9) & 0x7F) + 1980;
    int mes = (data >> 5) & 0x0F;
    int dia = data & 0x1F;

    stringstream ss;
    ss << setfill('0') << setw(2) << dia << "/"
       << setfill('0') << setw(2) << mes << "/"
       << ano;

    return ss.str();
}

// Converte a hora gravada 
string formatarHoraFAT(uint16_t hora) {
    int horas = (hora >> 11) & 0x1F;
    int minutos = (hora >> 5) & 0x3F;
    int segundos = (hora & 0x1F) * 2;

    stringstream ss;
    ss << setfill('0') << setw(2) << horas << ":"
       << setfill('0') << setw(2) << minutos << ":"
       << setfill('0') << setw(2) << segundos;

    return ss.str();
}

// Pega a data atual do sistema e converte para o formato usado pela FAT.
//  quando criado um novo arquivo dentro da imagem.
uint16_t obterDataFATAtual() {
    time_t agora = time(nullptr);
    tm* dataHora = localtime(&agora);

    int ano = dataHora->tm_year + 1900;
    int mes = dataHora->tm_mon + 1;
    int dia = dataHora->tm_mday;

    if (ano < 1980) {
        ano = 1980;
    }

    return static_cast<uint16_t>(((ano - 1980) << 9) | (mes << 5) | dia);
}

// Pega a hora atual do sistema e converte para o formato usado pela FAT.
uint16_t obterHoraFATAtual() {
    time_t agora = time(nullptr);
    tm* dataHora = localtime(&agora);

    int hora = dataHora->tm_hour;
    int minuto = dataHora->tm_min;
    int segundo = dataHora->tm_sec / 2;

    return static_cast<uint16_t>((hora << 11) | (minuto << 5) | segundo);
}

// Procura uma posição livre no diretório raiz para criar um novo arquivo.
// A entrada pode estar vazia ou marcada como apagada.
bool encontrarEntradaLivreDiretorio(fstream& imagem, const BootSector& boot, uint32_t& offsetEntradaLivre) {
    uint32_t inicioDiretorioRaiz = calcularInicioDiretorioRaiz(boot);

    imagem.clear();

    for (int i = 0; i < boot.entradasDiretorioRaiz; i++) {
        uint32_t offsetAtual = inicioDiretorioRaiz + (i * sizeof(EntradaDiretorio));

        EntradaDiretorio entrada;

        imagem.seekg(offsetAtual, ios::beg);
        imagem.read(reinterpret_cast<char*>(&entrada), sizeof(EntradaDiretorio));

        unsigned char primeiroByte = static_cast<unsigned char>(entrada.nome[0]);

        // 0x00 = entrada nunca usada. 0xE5 = entrada de arquivo apagado.
        if (primeiroByte == 0x00 || primeiroByte == 0xE5) {
            offsetEntradaLivre = offsetAtual;
            return true;
        }
    }

    return false;
}

// Procura clusters livres na FAT para armazenar o conteúdo de um novo arquivo.
// Um cluster livre aparece com valor 0x0000 na tabela FAT.
bool encontrarClustersLivres(
    fstream& imagem,
    const BootSector& boot,
    uint32_t quantidadeNecessaria,
    vector<uint16_t>& clustersLivres
) {
    clustersLivres.clear();

    if (quantidadeNecessaria == 0) {
        return true;
    }

    uint32_t totalClusters = calcularTotalClusters(boot);
    uint32_t ultimoCluster = totalClusters + 1;

    for (uint32_t cluster = 2; cluster <= ultimoCluster; cluster++) {
        uint16_t valorFAT = lerValorFAT16(imagem, boot, static_cast<uint16_t>(cluster));

        if (valorFAT == 0x0000) {
            clustersLivres.push_back(static_cast<uint16_t>(cluster));

            if (clustersLivres.size() == quantidadeNecessaria) {
                return true;
            }
        }
    }

    return false;
}

// Monta a cadeia de clusters na FAT.
// Cada cluster aponta para o próximo, e o último recebe 0xFFFF para marcar fim do arquivo.
void escreverCadeiaFAT(
    fstream& imagem,
    const BootSector& boot,
    const vector<uint16_t>& clusters
) {
    for (size_t i = 0; i < clusters.size(); i++) {
        uint16_t valor;

        if (i + 1 < clusters.size()) {
            valor = clusters[i + 1];
        } else {
            valor = 0xFFFF;
        }

        escreverValorFAT16EmTodasFATs(imagem, boot, clusters[i], valor);
    }

    imagem.flush();
}

// Escreve os bytes do arquivo externo nos clusters livres encontrados.
// Se sobrar espaço no último cluster, é preenchido com zero.
void escreverDadosNosClusters(
    fstream& imagem,
    const BootSector& boot,
    const vector<uint16_t>& clusters,
    const vector<char>& dadosArquivo,
    uint32_t tamanhoArquivo
) {
    uint32_t tamanhoCluster = boot.bytesPorSetor * boot.setoresPorCluster;
    uint32_t bytesEscritos = 0;

    for (uint16_t cluster : clusters) {
        uint32_t offsetCluster = calcularOffsetCluster(boot, cluster);

        vector<char> buffer(tamanhoCluster, 0);

        uint32_t bytesRestantes = tamanhoArquivo - bytesEscritos;
        uint32_t bytesParaCopiar = min(tamanhoCluster, bytesRestantes);

        if (bytesParaCopiar > 0) {
            memcpy(buffer.data(), dadosArquivo.data() + bytesEscritos, bytesParaCopiar);
        }

        imagem.clear();
        imagem.seekp(offsetCluster, ios::beg);
        imagem.write(buffer.data(), tamanhoCluster);

        bytesEscritos += bytesParaCopiar;
    }

    imagem.flush();
}

// Libera todos os clusters usados por um arquivo.
// Para apagar, é percorrido a cadeia na FAT e marcado cada cluster como livre.
void liberarCadeiaClusters(fstream& imagem, const BootSector& boot, uint16_t primeiroCluster) {
    uint16_t clusterAtual = primeiroCluster;

    while (clusterAtual >= 2 && clusterAtual < 0xFFF8) {
        uint16_t proximoCluster = lerValorFAT16(imagem, boot, clusterAtual);

        escreverValorFAT16EmTodasFATs(imagem, boot, clusterAtual, 0x0000);

        if (proximoCluster < 2 || proximoCluster >= 0xFFF8) {
            break;
        }

        clusterAtual = proximoCluster;
    }

    imagem.flush();
}

// Mostra algumas informações do Boot Sector para confirmar que a imagem foi lida corretamente.
void exibirInformacoesBootSector(const BootSector& boot) {
    cout << "\n[INFORMACOES DO BOOT SECTOR]\n";
    cout << "Bytes por setor: " << boot.bytesPorSetor << endl;
    cout << "Setores por cluster: " << static_cast<int>(boot.setoresPorCluster) << endl;
    cout << "Setores reservados: " << boot.setoresReservados << endl;
    cout << "Numero de FATs: " << static_cast<int>(boot.numeroDeFATs) << endl;
    cout << "Entradas no diretorio raiz: " << boot.entradasDiretorioRaiz << endl;
    cout << "Setores por FAT: " << boot.setoresPorFAT << endl;
}

// OPÇÃO 1: lista os arquivos do diretório raiz.
// Mostra o nome e o tamanho de cada arquivo encontrado.
void listarConteudoDisco(fstream& imagem, const BootSector& boot) {
    cout << "\n[LISTAR CONTEUDO DO DISCO]\n";

    uint32_t inicioDiretorioRaiz = calcularInicioDiretorioRaiz(boot);

    imagem.clear();
    imagem.seekg(inicioDiretorioRaiz, ios::beg);

    cout << left << setw(20) << "Nome do arquivo"
         << right << setw(15) << "Tamanho(bytes)" << endl;

    cout << "-----------------------------------\n";

    bool encontrouArquivo = false;

    for (int i = 0; i < boot.entradasDiretorioRaiz; i++) {
        EntradaDiretorio entrada;

        imagem.read(reinterpret_cast<char*>(&entrada), sizeof(EntradaDiretorio));

        unsigned char primeiroByte = static_cast<unsigned char>(entrada.nome[0]);

        if (primeiroByte == 0x00) {
            break;
        }

        if (entradaValida(entrada)) {
            string nomeArquivo = montarNomeArquivo(entrada);

            cout << left << setw(20) << nomeArquivo
                 << right << setw(15) << entrada.tamanhoArquivo << endl;

            encontrouArquivo = true;
        }
    }

    if (!encontrouArquivo) {
        cout << "Nenhum arquivo encontrado no diretorio raiz.\n";
    }
}

// OPÇÃO 2: mostra o conteúdo de um arquivo.
// Primeiro encontro o arquivo no diretório raiz e depois leio seus clusters na área de dados.
void listarConteudoArquivo(fstream& imagem, const BootSector& boot) {
    cout << "\n[LISTAR CONTEUDO DE UM ARQUIVO]\n";

    string nomeArquivo;

    cout << "Digite o nome do arquivo, exemplo TESTE.TXT: ";
    cin >> nomeArquivo;

    EntradaDiretorio entrada;

    if (!buscarArquivo(imagem, boot, nomeArquivo, entrada)) {
        cout << "Arquivo nao encontrado no diretorio raiz.\n";
        return;
    }

    if (entrada.tamanhoArquivo == 0) {
        cout << "Arquivo vazio.\n";
        return;
    }

    uint32_t tamanhoCluster = boot.bytesPorSetor * boot.setoresPorCluster;
    uint32_t bytesRestantes = entrada.tamanhoArquivo;
    uint16_t clusterAtual = entrada.primeiroCluster;

    vector<char> buffer(tamanhoCluster);

    cout << "\n----- CONTEUDO DE " << montarNomeArquivo(entrada) << " -----\n\n";

    while (clusterAtual >= 2 && clusterAtual < 0xFFF8 && bytesRestantes > 0) {
        uint32_t offsetCluster = calcularOffsetCluster(boot, clusterAtual);

        uint32_t bytesParaLer = min(tamanhoCluster, bytesRestantes);

        imagem.clear();
        imagem.seekg(offsetCluster, ios::beg);
        imagem.read(buffer.data(), bytesParaLer);

        cout.write(buffer.data(), bytesParaLer);

        bytesRestantes -= bytesParaLer;

        if (bytesRestantes > 0) {
            clusterAtual = lerValorFAT16(imagem, boot, clusterAtual);
        }
    }

    cout << "\n\n----- FIM DO ARQUIVO -----\n";
}

// OPÇÃO 3: exibe os atributos de um arquivo.
// Mostra datas, horas, tamanho, primeiro cluster e atributos como oculto, sistema e somente leitura.
void exibirAtributosArquivo(fstream& imagem, const BootSector& boot) {
    cout << "\n[EXIBIR ATRIBUTOS DE UM ARQUIVO]\n";

    string nomeArquivo;

    cout << "Digite o nome do arquivo, exemplo TESTE.TXT: ";
    cin >> nomeArquivo;

    EntradaDiretorio entrada;

    if (!buscarArquivo(imagem, boot, nomeArquivo, entrada)) {
        cout << "Arquivo nao encontrado no diretorio raiz.\n";
        return;
    }

    cout << "\n----- ATRIBUTOS DE " << montarNomeArquivo(entrada) << " -----\n";

    cout << "Nome: " << montarNomeArquivo(entrada) << endl;
    cout << "Tamanho: " << entrada.tamanhoArquivo << " bytes" << endl;
    cout << "Primeiro cluster: " << entrada.primeiroCluster << endl;

    cout << "Data de criacao: " << formatarDataFAT(entrada.dataCriacao) << endl;
    cout << "Hora de criacao: " << formatarHoraFAT(entrada.horaCriacao) << endl;

    cout << "Data da ultima modificacao: " << formatarDataFAT(entrada.dataModificacao) << endl;
    cout << "Hora da ultima modificacao: " << formatarHoraFAT(entrada.horaModificacao) << endl;

    cout << "\nAtributos:\n";
    cout << "Somente leitura: " << ((entrada.atributos & 0x01) ? "Sim" : "Nao") << endl;
    cout << "Oculto: " << ((entrada.atributos & 0x02) ? "Sim" : "Nao") << endl;
    cout << "Arquivo de sistema: " << ((entrada.atributos & 0x04) ? "Sim" : "Nao") << endl;

    cout << "----------------------------------------\n";
}

// OPÇÃO 4: renomeia um arquivo no diretório raiz.
// Aqui é alterado apenas os campos de nome e extensão da entrada do arquivo.
void renomearArquivo(fstream& imagem, const BootSector& boot) {
    cout << "\n[RENOMEAR ARQUIVO]\n";

    string nomeAtual;
    string novoNome;

    cout << "Digite o nome atual do arquivo, exemplo TESTE.TXT: ";
    cin >> nomeAtual;

    EntradaDiretorio entrada;
    uint32_t offsetEntrada;

    if (!buscarArquivoComOffset(imagem, boot, nomeAtual, entrada, offsetEntrada)) {
        cout << "Arquivo nao encontrado no diretorio raiz.\n";
        return;
    }

    cout << "Digite o novo nome do arquivo, exemplo NOVO.TXT: ";
    cin >> novoNome;

    EntradaDiretorio entradaExistente;

    if (converterParaMaiusculo(nomeAtual) != converterParaMaiusculo(novoNome) &&
        buscarArquivo(imagem, boot, novoNome, entradaExistente)) {
        cout << "Erro: ja existe um arquivo com esse nome.\n";
        return;
    }

    char nomeFormatado[8];
    char extensaoFormatada[3];

    if (!nomeValidoFAT83(novoNome, nomeFormatado, extensaoFormatada)) {
        return;
    }

    memcpy(entrada.nome, nomeFormatado, 8);
    memcpy(entrada.extensao, extensaoFormatada, 3);

    imagem.clear();
    imagem.seekp(offsetEntrada, ios::beg);
    imagem.write(reinterpret_cast<char*>(&entrada), sizeof(EntradaDiretorio));
    imagem.flush();

    cout << "Arquivo renomeado com sucesso para " << converterParaMaiusculo(novoNome) << ".\n";
}

// OPÇÃO 5: insere um arquivo externo dentro da imagem FAT16.
//  acha entrada livre, clusters livres, escreve os dados e atualiza a FAT.
void inserirArquivo(fstream& imagem, const BootSector& boot) {
    cout << "\n[INSERIR/CRIAR NOVO ARQUIVO]\n";

    string caminhoArquivoExterno;
    string nomeNaImagem;

    cout << "Digite o caminho do arquivo externo, exemplo novo.txt: ";
    cin >> caminhoArquivoExterno;

    cout << "Digite o nome para salvar na imagem FAT16, exemplo NOVO.TXT: ";
    cin >> nomeNaImagem;

    char nomeFormatado[8];
    char extensaoFormatada[3];

    if (!nomeValidoFAT83(nomeNaImagem, nomeFormatado, extensaoFormatada)) {
        return;
    }

    EntradaDiretorio entradaExistente;

    if (buscarArquivo(imagem, boot, nomeNaImagem, entradaExistente)) {
        cout << "Erro: ja existe um arquivo com esse nome na imagem.\n";
        return;
    }

    ifstream arquivoExterno(caminhoArquivoExterno, ios::binary | ios::ate);

    if (!arquivoExterno.is_open()) {
        cout << "Erro: nao foi possivel abrir o arquivo externo.\n";
        return;
    }

    streampos posicaoFinal = arquivoExterno.tellg();

    if (posicaoFinal < 0) {
        cout << "Erro ao obter o tamanho do arquivo externo.\n";
        arquivoExterno.close();
        return;
    }

    uint64_t tamanhoArquivo64 = static_cast<uint64_t>(posicaoFinal);

    if (tamanhoArquivo64 > 0xFFFFFFFFULL) {
        cout << "Erro: arquivo muito grande para FAT16.\n";
        arquivoExterno.close();
        return;
    }

    uint32_t tamanhoArquivo = static_cast<uint32_t>(tamanhoArquivo64);

    vector<char> dadosArquivo(tamanhoArquivo);

    arquivoExterno.seekg(0, ios::beg);

    if (tamanhoArquivo > 0) {
        arquivoExterno.read(dadosArquivo.data(), tamanhoArquivo);
    }

    arquivoExterno.close();

    uint32_t tamanhoCluster = boot.bytesPorSetor * boot.setoresPorCluster;
    uint32_t quantidadeClusters = 0;

    if (tamanhoArquivo > 0) {
        quantidadeClusters = (tamanhoArquivo + tamanhoCluster - 1) / tamanhoCluster;
    }

    uint32_t offsetEntradaLivre;

    if (!encontrarEntradaLivreDiretorio(imagem, boot, offsetEntradaLivre)) {
        cout << "Erro: nao ha espaco livre no diretorio raiz.\n";
        return;
    }

    vector<uint16_t> clustersLivres;

    if (!encontrarClustersLivres(imagem, boot, quantidadeClusters, clustersLivres)) {
        cout << "Erro: nao ha clusters livres suficientes na imagem.\n";
        return;
    }

    if (tamanhoArquivo > 0) {
        // Primeiro é escrito o conteúdo nos clusters e depois atualizado a FAT com a sequência deles.
        escreverDadosNosClusters(imagem, boot, clustersLivres, dadosArquivo, tamanhoArquivo);
        escreverCadeiaFAT(imagem, boot, clustersLivres);
    }

    EntradaDiretorio novaEntrada;
    memset(&novaEntrada, 0, sizeof(EntradaDiretorio));

    memcpy(novaEntrada.nome, nomeFormatado, 8);
    memcpy(novaEntrada.extensao, extensaoFormatada, 3);

    novaEntrada.atributos = 0x20;
    novaEntrada.tempoCriacaoDecimos = 0;

    novaEntrada.horaCriacao = obterHoraFATAtual();
    novaEntrada.dataCriacao = obterDataFATAtual();

    novaEntrada.dataUltimoAcesso = novaEntrada.dataCriacao;
    novaEntrada.clusterAlto = 0;
    novaEntrada.horaModificacao = novaEntrada.horaCriacao;
    novaEntrada.dataModificacao = novaEntrada.dataCriacao;

    if (!clustersLivres.empty()) {
        novaEntrada.primeiroCluster = clustersLivres[0];
    } else {
        novaEntrada.primeiroCluster = 0;
    }

    novaEntrada.tamanhoArquivo = tamanhoArquivo;

    imagem.clear();
    imagem.seekp(offsetEntradaLivre, ios::beg);

    // Por fim, grava a nova entrada no diretório raiz.
    imagem.write(reinterpret_cast<char*>(&novaEntrada), sizeof(EntradaDiretorio));
    imagem.flush();

    cout << "Arquivo inserido com sucesso como " << converterParaMaiusculo(nomeNaImagem) << ".\n";
    cout << "Tamanho: " << tamanhoArquivo << " bytes.\n";

    if (!clustersLivres.empty()) {
        cout << "Primeiro cluster: " << clustersLivres[0] << ".\n";
    }
}

// OPÇÃO 6: remove um arquivo da imagem.
//  libero os clusters na FAT e marca a entrada do diretório com 0xE5.
void apagarArquivo(fstream& imagem, const BootSector& boot) {
    cout << "\n[APAGAR/REMOVER ARQUIVO]\n";

    string nomeArquivo;

    cout << "Digite o nome do arquivo a remover, exemplo TESTE.TXT: ";
    cin >> nomeArquivo;

    EntradaDiretorio entrada;
    uint32_t offsetEntrada;

    if (!buscarArquivoComOffset(imagem, boot, nomeArquivo, entrada, offsetEntrada)) {
        cout << "Arquivo nao encontrado no diretorio raiz.\n";
        return;
    }

    if (entrada.primeiroCluster >= 2 && entrada.tamanhoArquivo > 0) {
        // Antes de marcar o arquivo como apagado, é liberado os clusters dele na FAT.
        liberarCadeiaClusters(imagem, boot, entrada.primeiroCluster);
    }

    // Em FAT, 0xE5 no primeiro byte do nome marca a entrada como apagada.
    unsigned char marcadorApagado = 0xE5;

    imagem.clear();
    imagem.seekp(offsetEntrada, ios::beg);
    imagem.write(reinterpret_cast<char*>(&marcadorApagado), sizeof(unsigned char));
    imagem.flush();

    cout << "Arquivo " << converterParaMaiusculo(nomeArquivo) << " removido com sucesso.\n";
}

// Função principal: abre a imagem, lê o Boot Sector e mantém o menu rodando.
int main() {
        // Nome da imagem FAT16 que o programa vai manipular.
    string nomeImagem = "disco1.img";

    fstream imagem(nomeImagem, ios::in | ios::out | ios::binary);

    if (!imagem.is_open()) {
        cout << "Erro: nao foi possivel abrir a imagem de disco: " << nomeImagem << endl;
        cout << "Verifique se o arquivo esta na mesma pasta do programa.\n";
        return 1;
    }

    cout << "Imagem de disco aberta com sucesso: " << nomeImagem << endl;

    BootSector boot = lerBootSector(imagem);
    exibirInformacoesBootSector(boot);

    int opcao;

    // O menu fica em repetição para permitir várias operações sem reiniciar o programa.
    do {
        cout << "\n========== MANIPULADOR FAT16 ==========\n";
        cout << "1 - Listar conteudo do disco\n";
        cout << "2 - Listar conteudo de um arquivo\n";
        cout << "3 - Exibir atributos de um arquivo\n";
        cout << "4 - Renomear arquivo\n";
        cout << "5 - Inserir/criar novo arquivo\n";
        cout << "6 - Apagar/remover arquivo\n";
        cout << "0 - Sair\n";
        cout << "Escolha uma opcao: ";
        cin >> opcao;

        switch (opcao) {
            case 1:
                listarConteudoDisco(imagem, boot);
                break;
            case 2:
                listarConteudoArquivo(imagem, boot);
                break;
            case 3:
                exibirAtributosArquivo(imagem, boot);
                break;
            case 4:
                renomearArquivo(imagem, boot);
                break;
            case 5:
                inserirArquivo(imagem, boot);
                break;
            case 6:
                apagarArquivo(imagem, boot);
                break;
            case 0:
                cout << "Encerrando o programa...\n";
                break;
            default:
                cout << "Opcao invalida. Tente novamente.\n";
        }

    } while (opcao != 0);

    imagem.close();

    return 0;
}

// Para compilar:
// g++ main.cpp -o main

// Para executar:
// ./main
