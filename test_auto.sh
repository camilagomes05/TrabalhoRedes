# Função para listar arquivos em 3 peers
list_3_peers() {
  echo ""
  echo "--- Verificando estado dos 3 peers ---"
  echo ">>> Peer 1:"
  MSYS_NO_PATHCONV=1 docker-compose exec peer1 ls -l /app/tmp
  echo ">>> Peer 2:"
  MSYS_NO_PATHCONV=1 docker-compose exec peer2 ls -l /app/tmp
  echo ">>> Peer 3:"
  MSYS_NO_PATHCONV=1 docker-compose exec peer3 ls -l /app/tmp
  echo "------------------------------------"
  echo ""
}

echo ""
echo "INICIANDO TESTE AUTOMÁTICO DO P2P SYNC"

# 1. VERIFICAÇÃO DO ESTADO INICIAL
echo "[PASSO 1/8] Verificando o estado inicial dos diretórios..."
list_3_peers
sleep 3

# 2. ADIÇÃO AUTOMÁTICA DE ARQUIVO
FILENAME="auto_file_$(date +%s).txt"
echo "[PASSO 2/8] Adicionando arquivo '$FILENAME' no peer1..."
MSYS_NO_PATHCONV=1 docker-compose exec peer1 sh -c "echo 'Este arquivo foi criado automaticamente às $(date)' > /app/tmp/$FILENAME"
echo "Aguardando 10 segundos para a sincronização ocorrer..."
sleep 10

# 3. VERIFICAÇÃO PÓS-ADIÇÃO
echo "[PASSO 3/8] Verificando se o arquivo foi sincronizado para todos os peers..."
list_3_peers
echo "O arquivo '$FILENAME' deve aparecer em todos os 3 peers."
sleep 5

# 4. REMOÇÃO AUTOMÁTICA DE ARQUIVO
echo "[PASSO 4/8] Removendo o arquivo '$FILENAME' a partir do peer2..."
MSYS_NO_PATHCONV=1 docker-compose exec peer2 rm /app/tmp/$FILENAME
echo "Aguardando 10 segundos para a remoção ser sincronizada..."
sleep 10

# 5. VERIFICAÇÃO PÓS-REMOÇÃO
echo "[PASSO 5/8] Verificando se o arquivo foi removido de todos os peers..."
list_3_peers
echo "Os diretórios de todos os peers devem estar vazios novamente."


# TESTE DE ADIÇÃO DE NODO 
echo ""
echo "  INICIANDO TESTE DE ADIÇÃO DE NOVO NODO  "

# 6. DERRUBANDO A REDE ANTIGA
echo "[PASSO 6/8] Encerrando a rede de 3 peers..."
docker-compose down -v
sleep 3

# 7. INICIANDO A NOVA REDE COM 4 PEERS
echo "[PASSO 7/8] Iniciando a nova rede com 4 peers usando 'docker-compose-4peers.yml'..."
# Usamos a flag -f para especificar o novo arquivo de configuração
docker-compose -f docker-compose-4peers.yml up -d
echo "Aguardando 15 segundos para a estabilização da nova rede..."
sleep 15

# 8. VERIFICANDO A SINCRONIZAÇÃO COM O NOVO PEER4
echo "[PASSO 8/8] Verificando a sincronização com o novo peer4..."
NEW_FILENAME="arquivo_na_rede_maior.txt"
echo "Adicionando arquivo '$NEW_FILENAME' no peer1..."
MSYS_NO_PATHCONV=1 docker-compose -f docker-compose-4peers.yml exec peer1 sh -c "echo 'conteúdo' > /app/tmp/$NEW_FILENAME"
echo "Aguardando 10 segundos para a sincronização..."
sleep 10

echo "Verificando se o arquivo apareceu no peer4..."
MSYS_NO_PATHCONV=1 docker-compose -f docker-compose-4peers.yml exec peer4 ls -l /app/tmp
echo "O arquivo '$NEW_FILENAME' deve estar listado acima, provando que o novo nodo foi integrado."
echo ""

# Limpeza final
echo "Encerrando a rede de 4 peers..."
docker-compose -f docker-compose-4peers.yml down -v

echo ""
echo "TESTE COMPLETO CONCLUÍDO"
