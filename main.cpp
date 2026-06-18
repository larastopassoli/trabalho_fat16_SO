#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cctype>

using namespace std;

#pragma pack(push, 1)

// Estrutura do Boot Sector da FAT16
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

// Estrutura de uma entrada do diretório raiz
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

string limparEspacos(const string& texto) {
    size_t inicio = texto.find_first_not_of(' ');
    size_t fim = texto.find_last_not_of(' ');

    if (inicio == string::npos) {
        return "";
    }

    return texto.substr(inicio, fim - inicio + 1);
}

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

string converterParaMaiusculo(string texto) {
    transform(texto.begin(), texto.end(), texto.begin(),
              [](unsigned char c) {
                  return static_cast<char>(toupper(c));
              });

    return texto;
}

bool entradaValida(const EntradaDiretorio& entrada) {
    unsigned char primeiroByte = static_cast<unsigned char>(entrada.nome[0]);

    // 0x00 indica que não há mais entradas no diretório
    if (primeiroByte == 0x00) {
        return false;
    }

    // 0xE5 indica arquivo apagado
    if (primeiroByte == 0xE5) {
        return false;
    }

    // 0x0F indica entrada de nome longo, que vamos ignorar
    if (entrada.atributos == 0x0F) {
        return false;
    }

    // 0x08 indica rótulo de volume, não é arquivo comum
    if (entrada.atributos & 0x08) {
        return false;
    }

    // 0x10 indica diretório, e o trabalho não considera subdiretórios
    if (entrada.atributos & 0x10) {
        return false;
    }

    return true;
}

BootSector lerBootSector(fstream& imagem) {
    BootSector boot;

    imagem.clear();
    imagem.seekg(0, ios::beg);
    imagem.read(reinterpret_cast<char*>(&boot), sizeof(BootSector));

    return boot;
}

uint32_t calcularInicioDiretorioRaiz(const BootSector& boot) {
    uint32_t setorInicioDiretorioRaiz = boot.setoresReservados +
                                        (boot.numeroDeFATs * boot.setoresPorFAT);

    return setorInicioDiretorioRaiz * boot.bytesPorSetor;
}

uint32_t calcularSetoresDiretorioRaiz(const BootSector& boot) {
    uint32_t tamanhoDiretorioRaiz = boot.entradasDiretorioRaiz * sizeof(EntradaDiretorio);

    return (tamanhoDiretorioRaiz + boot.bytesPorSetor - 1) / boot.bytesPorSetor;
}

uint32_t calcularInicioAreaDados(const BootSector& boot) {
    uint32_t setoresDiretorioRaiz = calcularSetoresDiretorioRaiz(boot);

    uint32_t setorInicioAreaDados = boot.setoresReservados +
                                    (boot.numeroDeFATs * boot.setoresPorFAT) +
                                    setoresDiretorioRaiz;

    return setorInicioAreaDados * boot.bytesPorSetor;
}

uint32_t calcularOffsetCluster(const BootSector& boot, uint16_t cluster) {
    uint32_t inicioAreaDados = calcularInicioAreaDados(boot);
    uint32_t tamanhoCluster = boot.bytesPorSetor * boot.setoresPorCluster;

    return inicioAreaDados + ((cluster - 2) * tamanhoCluster);
}

uint16_t lerValorFAT16(fstream& imagem, const BootSector& boot, uint16_t cluster) {
    uint32_t inicioFAT = boot.setoresReservados * boot.bytesPorSetor;
    uint32_t offset = inicioFAT + (cluster * 2);

    uint16_t valor;

    imagem.clear();
    imagem.seekg(offset, ios::beg);
    imagem.read(reinterpret_cast<char*>(&valor), sizeof(uint16_t));

    return valor;
}

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

void exibirInformacoesBootSector(const BootSector& boot) {
    cout << "\n[INFORMACOES DO BOOT SECTOR]\n";
    cout << "Bytes por setor: " << boot.bytesPorSetor << endl;
    cout << "Setores por cluster: " << static_cast<int>(boot.setoresPorCluster) << endl;
    cout << "Setores reservados: " << boot.setoresReservados << endl;
    cout << "Numero de FATs: " << static_cast<int>(boot.numeroDeFATs) << endl;
    cout << "Entradas no diretorio raiz: " << boot.entradasDiretorioRaiz << endl;
    cout << "Setores por FAT: " << boot.setoresPorFAT << endl;
}

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

        // Se encontrou 0x00, não há mais arquivos depois dessa entrada
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

void exibirAtributosArquivo() {
    cout << "\n[EXIBIR ATRIBUTOS DE UM ARQUIVO]\n";
    cout << "Funcao ainda sera implementada.\n";
}

void renomearArquivo() {
    cout << "\n[RENOMEAR ARQUIVO]\n";
    cout << "Funcao ainda sera implementada.\n";
}

void inserirArquivo() {
    cout << "\n[INSERIR/CRIAR NOVO ARQUIVO]\n";
    cout << "Funcao ainda sera implementada.\n";
}

void apagarArquivo() {
    cout << "\n[APAGAR/REMOVER ARQUIVO]\n";
    cout << "Funcao ainda sera implementada.\n";
}

int main() {
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
                exibirAtributosArquivo();
                break;
            case 4:
                renomearArquivo();
                break;
            case 5:
                inserirArquivo();
                break;
            case 6:
                apagarArquivo();
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



//g++ main.cpp -o main
//./main