#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <cstdint>
#include <cstring>

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

    imagem.seekg(0, ios::beg);
    imagem.read(reinterpret_cast<char*>(&boot), sizeof(BootSector));

    return boot;
}

uint32_t calcularInicioDiretorioRaiz(const BootSector& boot) {
    uint32_t setorInicioDiretorioRaiz = boot.setoresReservados + 
                                        (boot.numeroDeFATs * boot.setoresPorFAT);

    return setorInicioDiretorioRaiz * boot.bytesPorSetor;
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

void listarConteudoArquivo() {
    cout << "\n[LISTAR CONTEUDO DE UM ARQUIVO]\n";
    cout << "Funcao ainda sera implementada.\n";
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
        cout << "Verifique se o arquivo disco.img esta na mesma pasta do programa.\n";
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
                listarConteudoArquivo();
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